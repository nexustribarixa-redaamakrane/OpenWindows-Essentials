#ifndef _SHIM_WINDOWS_H_
#define _SHIM_WINDOWS_H_

#include <stddef.h>
#include <stdint.h>

typedef void *PVOID;
typedef void *HANDLE;
typedef unsigned long DWORD;
typedef long LONG;
typedef unsigned short WCHAR;
typedef int BOOL;

#define MAX_PATH 260
#define INVALID_HANDLE_VALUE ((void *)(intptr_t)-1)

#define GENERIC_READ  (0x80000000UL)
#define GENERIC_WRITE (0x40000000UL)
#define CREATE_ALWAYS  2
#define FILE_ATTRIBUTE_NORMAL         0x00000080UL
#define FILE_FLAG_DELETE_ON_CLOSE     0x04000000UL

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG  HighPart;
    };
    long long QuadPart;
} LARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct {
        DWORD LowPart;
        DWORD HighPart;
    };
    unsigned long long QuadPart;
} ULARGE_INTEGER;

/* INIT_ONCE: the vendored cairo-atomic-private.h uses the synchronous
 * one-shot initialisation helpers when _WIN32 is defined. We fold them to
 * trivially-satisfied calls (single-threaded freestanding build). */
typedef struct _INIT_ONCE { PVOID Ptr; } INIT_ONCE, *PINIT_ONCE;

#define INIT_ONCE_STATIC_INIT { 0 }
#define INIT_ONCE_CHECK_ONLY 0x00000001UL

static inline BOOL InitOnceBeginInitialize(PINIT_ONCE InitOnce, unsigned long dwFlags, BOOL *fPending, void **lpContext)
{
    (void)InitOnce;
    (void)dwFlags;
    (void)lpContext;
    if (fPending) {
        *fPending = FALSE;
    }
    return TRUE;
}

static inline BOOL InitOnceComplete(PINIT_ONCE InitOnce, unsigned long dwFlags, void *lpContext)
{
    (void)InitOnce;
    (void)dwFlags;
    (void)lpContext;
    return TRUE;
}

/* ---- Interlocked intrinsics (single-threaded freestanding) ---------- */
static inline LONG InterlockedIncrement(LONG volatile *v)
{
    return (LONG)(++(*v));
}
static inline LONG InterlockedDecrement(LONG volatile *v)
{
    return (LONG)(--(*v));
}
static inline LONG InterlockedCompareExchange(LONG volatile *dst, LONG xchg, LONG cmp)
{
    if (*dst == cmp)
        *dst = xchg;
    return cmp;
}
static inline void *InterlockedCompareExchangePointer(void *volatile *dst, void *xchg, void *cmp)
{
    if (*dst == cmp)
        *dst = xchg;
    return cmp;
}
#define MemoryBarrier() __asm__ __volatile__ ("" : : : "memory")

/* ---- Win32 API entry points (implemented/failed-safe in ow_crt.c) ---- */
void *CreateFileW(const void *name, unsigned int access, unsigned int share,
                  void *sa, unsigned int disp, unsigned int flags, void *tpl);
int CloseHandle(void *h);
int DeleteFileW(const void *p);
unsigned int GetTempPathW(unsigned int n, void *buf);
unsigned int GetTempFileNameW(const void *dir, const void *prefix,
                              unsigned int u, void *name);
int QueryPerformanceCounter(void *lp);
int QueryPerformanceFrequency(void *lp);
void *_wfopen(const unsigned short *path, const unsigned short *mode);

#endif /* _SHIM_WINDOWS_H_ */
