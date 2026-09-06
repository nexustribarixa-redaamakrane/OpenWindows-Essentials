/*
 * owcrt64.h - owcrt64.owd ABI mirror (Core C/C++ Runtime Environment)
 *
 * Freestanding C99 ABI for the OpenWindows core runtime: the standard
 * allocator bridge onto kernel64.owd, process startup/exit and atexit
 * registries, TLS block management, the freestanding string/memory
 * primitives, and the C++ ABI support stubs. Every allocation below comes
 * out of kernel64.owd page descriptors (k64_request_memory_block), never a
 * host malloc.
 *
 * Conforms to OWD1 binary format (Extensions/owd_format.h).
 * C99 freestanding - <stdint.h>/<stdbool.h>/<stddef.h>/<limits.h> only.
 */

#ifndef OWE_OWCRT64_H
#define OWE_OWCRT64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  Status Codes - BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A7FF): Fatal module faults                    */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal degradations                 */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                     */
/* ------------------------------------------------------------------ */

typedef uint32_t owcrt64_status_t;

#define OWCRT64_OK                         0x00000000u  /* Success        */
/* B+ Fatal */
#define OWCRT64_BAN_K64_BOOT               0x0011A2A0u  /* k64 API dead    */
#define OWCRT64_BAN_OUT_OF_MEMORY          0x0011A2A1u  /* heap exhausted  */
#define OWCRT64_BAN_CORRUPT_HEAP           0x0011A2A2u  /* canary/header  */
#define OWCRT64_BAN_PURE_VIRTUAL           0x0011A2A3u  /* __cxa_pure_virt */
#define OWCRT64_BAN_TLS_OVERFLOW           0x0011A2A4u  /* TLS table full */
/* S+ Soft */
#define OWCRT64_ERR_UNINITIALIZED          0x0011AEA0u  /* Not init yet   */
#define OWCRT64_ERR_NO_ATEXIT              0x0011AEA1u  /* atexit full    */
#define OWCRT64_ERR_NO_TLS_SLOT            0x0011AEA2u  /* slot table full*/
#define OWCRT64_ERR_BAD_PARAM              0x0011AEA3u  /* Bad arg        */

/* ------------------------------------------------------------------ */
/*  Heap geometry                                                      */
/* ------------------------------------------------------------------ */

#define OWCRT64_SEGMENT_DEFAULT    0x40000u   /* 256 KiB arena segments */
#define OWCRT64_SEGMENT_MIN        0x1000u    /* 4 KiB min segment     */
#define OWCRT64_ALIGN             16u         /* 16-byte alignment     */
#define OWCRT64_SMALL_BUCKETS      16u        /* segregation classes   */
#define OWCRT64_LARGE_THRESHOLD    0x20000u   /* dedicated-block bound */
#define OWCRT64_HEADER_SIZE        16u         /* alloc header bytes    */
#define OWCRT64_CANARY            0xEEEEEE00u  /* payload tail canary   */

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

owcrt64_status_t owcrt64_module_init(void);
owcrt64_status_t owcrt64_module_shutdown(void);
uint32_t owcrt64_abi_version(void);
uint16_t owcrt64_abi_major(void);
uint16_t owcrt64_abi_minor(void);
const uint8_t *owcrt64_ident(void);

/* ------------------------------------------------------------------ */
/*  Process startup / exit                                             */
/* ------------------------------------------------------------------ */

/* Run registered initialization hooks (module constructors), then all
 * atexit handlers in reverse order and return. Safe to call repeatedly. */
owcrt64_status_t owcrt64_start(void);

/* Run the atexit chain immediately. Returns the passed `code` as a status
 * mapping (0 -> OWCRT64_OK, else OWCRT64_ERR_BAD_PARAM-compatible). */
owcrt64_status_t owcrt64_exit(int32_t code);

/* Register a no-argument exit handler. Returns OWCRT64_OK or
 * OWCRT64_ERR_NO_ATEXIT when the registry is full. */
owcrt64_status_t owcrt64_atexit(void (*fn)(void));

/* Register a destructor with one argument (__cxa_atexit contract). */
int __cxa_atexit(void (*fn)(void *), void *arg, void *dso);
void __cxa_finalize(void *dso);
void __cxa_pure_virtual(void);

/* ------------------------------------------------------------------ */
/*  Memory allocation bridge (kernel64.owd)                            */
/*  All returned pointers are OWCRT64_ALIGN-aligned. Each allocation    */
/*  carries a 16-byte header stamped by us and a tail canary; large      */
/*  allocations get a dedicated kernel segment with guard page.          */
/* ------------------------------------------------------------------ */

void *ow_malloc(size_t size);
void *ow_calloc(size_t nmemb, size_t size);
void *ow_realloc(void *ptr, size_t size);
void  ow_free(void *ptr);
size_t ow_malloc_usable_size(const void *ptr);

/* Heap status counters (freestanding tx/rx byte counters). */
typedef struct {
    uint64_t bytes_allocated;   /* outstanding user bytes              */
    uint64_t bytes_segmented;   /* total kernel pages claimed          */
    uint64_t allocs;            /* successful allocations              */
    uint64_t frees;             /* released allocations                */
    uint64_t guard_faults;      /* canary overruns caught              */
    uint32_t segments;          /* active kernel segments              */
} owcrt64_heap_stats_t;

owcrt64_status_t owcrt64_heap_stats(owcrt64_heap_stats_t *out);

/* ------------------------------------------------------------------ */
/*  TLS block registry                                                 */
/*  A slot is a per-thread instance of a sized block. Threads are        */
/*  identified by the caller's native thread handle (uintptr_t).        */
/* ------------------------------------------------------------------ */

#define OWCRT64_TLS_SLOTS     16u
#define OWCRT64_TLS_THREADS  64u

owcrt64_status_t owcrt64_tls_slot_create(size_t size, uint32_t *out_slot);
owcrt64_status_t owcrt64_tls_slot_destroy(uint32_t slot);
void *owcrt64_tls_get(uint32_t slot, uintptr_t thread);
/* Release all blocks bound to `thread`; call at thread exit. */
owcrt64_status_t owcrt64_tls_thread_exit(uintptr_t thread);

/* ------------------------------------------------------------------ */
/*  Memory primitives                                                  */
/* ------------------------------------------------------------------ */

void *ow_memcpy(void *dst, const void *src, size_t n);
void *ow_memset(void *dst, int c, size_t n);
void *ow_memmove(void *dst, const void *src, size_t n);
int   ow_memcmp(const void *a, const void *b, size_t n);

/* ------------------------------------------------------------------ */
/*  String primitives                                                  */
/* ------------------------------------------------------------------ */

size_t ow_strlen(const char *s);
int    ow_strcmp(const char *a, const char *b);
int    ow_strncmp(const char *a, const char *b, size_t n);
char  *ow_strcpy(char *dst, const char *src);
char  *ow_strncpy(char *dst, const char *src, size_t n);

/* Minimal freestanding formatter. Supported specifiers:
 *   %[flags][width][.prec]d i u o x X c s p %%
 *   flags: - + space # 0   width: digits   precision: .digits
 * Returns the number of characters that would have been written had `cap`
 * been unlimited; NUL-terminates when cap > 0. */
size_t ow_snprintf(char *out, size_t cap, const char *fmt, ...);

/* SUTF-8 helpers (see Extensions/sutf8.h for the canonical codec). */
size_t ow_utf8_seqlen(uint32_t cp);              /* 1..4 for valid range */
uint32_t ow_utf8_decode(const uint8_t *s, size_t n, size_t *consumed);
size_t ow_utf8_encode(uint32_t cp, uint8_t *out); /* 0 if out of range  */
size_t ow_utf8_strlen(const uint8_t *s);          /* codepoint count    */

#ifdef __cplusplus
}
#endif

#endif /* OWE_OWCRT64_H */