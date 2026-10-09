/*
 * kernel64.h - OpenWindows Base API (.owd)
 *
 * Public API wrappers for the OpenWindows kernel64 dynamic library.
 * Provides system time, memory management, and exception raising
 * routed through the OWRP ring portal gate.
 *
 * Conforms to OWD1 binary format (Extensions/owd_format.h).
 * C99 freestanding - no heap, no hosted libc.
 */

#ifndef KERNEL64_H
#define KERNEL64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Version                                                            */
/* ------------------------------------------------------------------ */

#define K64_VERSION_MAJOR    1u
#define K64_VERSION_MINOR    0u
#define K64_VERSION_PATCH    0u

/* ------------------------------------------------------------------ */
/*  OWD1 Binary Header Constants                                       */
/*  (magic 0x4F574431, header 192 bytes, see owd_format.h)            */
/* ------------------------------------------------------------------ */

#define K64_LIB_TYPE         OWD_LIBTYPE_HYBRID   /* 0x02 user+kernel  */
#define K64_TARGET_ARCH      0x02u                /* x86_64            */
#define K64_ALIGNMENT_LOG2   12u                  /* 4 KiB pages       */

/* ------------------------------------------------------------------ */
/*  Status Codes — BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A77F): Fatal system faults                   */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal warnings                    */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
/* ------------------------------------------------------------------ */

typedef uint32_t k64_status_t;

#define K64_OK                          0x00000000u  /* Success              */
/* B+ Fatal */
#define K64_ERR_NOT_INITIALIZED         0x0011A000u  /* API not init'd       */
#define K64_ERR_INVALID_PARAM           0x0011A001u  /* Bad argument         */
#define K64_ERR_OUT_OF_MEMORY           0x0011A002u  /* Memory block avail   */
#define K64_ERR_INVALID_POINTER         0x0011A003u  /* Bad pointer arg      */
#define K64_ERR_GATE_FAILED             0x0011A004u  /* OWRP gate fault      */
#define K64_ERR_PRIVILEGE               0x0011A005u  /* Insufficient ring    */
#define K64_ERR_SYSTEM_FAULT            0x0011A006u  /* Unrecoverable fault  */
/* S+ Soft */
#define K64_ERR_BANCODE_RAISED          0x0011AE00u  /* Exception dispatched */

/* ------------------------------------------------------------------ */
/*  BANcode Constants (from Documents/BANcode/bancode_all.h)          */
/* ------------------------------------------------------------------ */

#define K64_BANCODE_START       0x0011A000u
#define K64_BANCODE_END         0x0011A7FFu
#define K64_TRAP_MIN            0x7FFFFFF0u
#define K64_TRAP_MAX            0x7FFFFFFEu

/* ------------------------------------------------------------------ */
/*  Memory Block Descriptor (returned by k64_request_memory_block)     */
/* ------------------------------------------------------------------ */

typedef struct {
    void    *base;             /* Base address of allocated block    */
    size_t   size;             /* Usable size in bytes               */
    uint32_t flags;            /* Block flags                        */
    uint32_t checksum;         /* CRC32c of block header             */
} k64_mem_block_t;

#define K64_MEM_FLAG_NORMAL     0x00000000u
#define K64_MEM_FLAG_DMA        0x00000001u  /* DMA-coherent          */
#define K64_MEM_FLAG_ZEROED     0x00000002u  /* Block was zero-filled  */
#define K64_MEM_FLAG_GUARD      0x00000004u  /* Guard page appended    */

/* ------------------------------------------------------------------ */
/*  System Time                                                        */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t    tsc;            /* Raw TSC value                     */
    uint64_t    ticks_per_ms;   /* TSC frequency calibration         */
    uint32_t    seconds;        /* Uptime in seconds                 */
    uint32_t    milliseconds;   /* Sub-second milliseconds           */
} k64_system_time_t;

/* ------------------------------------------------------------------ */
/*  Exception Context                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t    bancode;        /* BANcode being raised              */
    uint64_t    subcode;        /* Vendor-specific sub-identifier    */
    uint64_t    timestamp;      /* TSC at exception time             */
    uint8_t     severity;       /* 0=info, 1=warn, 2=critical, 3=fatal */
    uint8_t     reserved[7];
} k64_exception_context_t;

/* ------------------------------------------------------------------ */
/*  Guard Page Size                                                    */
/* ------------------------------------------------------------------ */

#define K64_GUARD_PAGE_SIZE     0x1000u  /* 4 KiB guard page           */

/* ------------------------------------------------------------------ */
/*  API                                                                */
/* ------------------------------------------------------------------ */

/*
 * Initialize the kernel64 API layer.
 * Must be called before any other k64_* function.
 * Connects to the OWRP ring portal internally.
 * Returns K64_OK on success.
 */
k64_status_t k64_initialize_api(void);

/*
 * Query the current system time.
 * Fills `out_timestamp` with the raw TSC value.
 * Returns K64_OK or K64_ERR_GATE_FAILED.
 */
k64_status_t k64_query_system_time(uint64_t *out_timestamp);

/*
 * Request a memory block of at least `size_bytes` bytes.
 * The returned pointer is page-aligned and usable immediately.
 * A guard page is appended after the block for overflow detection.
 * Returns K64_OK on success, fills `out_ptr`.
 */
k64_status_t k64_request_memory_block(size_t size_bytes, void **out_ptr);

/*
 * Release a previously allocated memory block.
 * The pointer must have been obtained from k64_request_memory_block.
 * Returns K64_OK or K64_ERR_INVALID_POINTER.
 */
k64_status_t k64_release_memory_block(void *ptr);

/*
 * Raise a system exception routed to the Banhammer driver.
 * Constructs a k64_exception_context_t and dispatches through OWRP
 * to the BANcode handler (sys_nr OWRP_SYS_BAN_RAISE).
 * `bancode` must be in the B+ range (0x0011A000-0x0011A7FF).
 * `subcode` is an opaque vendor-specific value.
 * Returns K64_ERR_BANCODE_RAISED on success (exception was dispatched).
 */
k64_status_t k64_raise_system_exception(uint32_t bancode, uint64_t subcode);

/*
 * Get detailed system time info (uptime + TSC calibration).
 * Fills the k64_system_time_t structure.
 */
k64_status_t k64_get_system_time_full(k64_system_time_t *out_time);

/*
 * Check if the API is initialized and the OWRP gate is available.
 */
bool k64_api_ready(void);

/*
 * Get the library version.
 */
void k64_get_version(uint32_t *major, uint32_t *minor, uint32_t *patch);

#endif /* KERNEL64_H */
