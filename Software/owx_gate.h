/* owx_gate.h - the Ring 3 system call gateway, as an Essentials .owx sees it.
 *
 * Every OpenWindows user-mode program reaches the kernel the same way: the call
 * number goes in RAX, the three arguments in RCX/RDX/R8, and `int $0x80` traps
 * to the kernel's OwHalCommonInterruptHandler(), which forwards to
 * OwSyscallDispatch() (OpenWindows/syscall/dispatcher.c).
 *
 * This header is the Essentials-side twin of OpenWindows/emergency/ow_gate.h.
 * It is a separate copy rather than an include of that one on purpose: the
 * emergency pair is built by the kernel repository and must not depend on the
 * Essentials checkout, and these programs are built by the Essentials
 * repository and must not depend on the kernel checkout.  Both copies name the
 * same numbers, and the kernel-side host test asserts they match
 * inc/ow_syscall.h, so the duplication is checked rather than trusted.
 *
 * C99 freestanding.  No libc, no heap, no CRT: an .owx runs in a Ring 3 window
 * that holds nothing but the image's own sections, so everything below is built
 * from the trap and the console syscall. */
#ifndef OWX_GATE_H
#define OWX_GATE_H

#include <stdint.h>

/* ---- System call numbers (must match OpenWindows/inc/ow_syscall.h) -------- */
#define OWX_GATE_UART_WRITE   0x01BL
#define OWX_GATE_PS_YIELD     0x00DL
#define OWX_GATE_PS_EXIT      0x00EL
#define OWX_GATE_PS_GET_PID   0x00FL
#define OWX_GATE_PS_SLEEP     0x018L
#define OWX_GATE_PS_SPAWN_OWX 0x020L
#define OWX_GATE_UART_READ    0x021L
#define OWX_GATE_VFS_READ     0x005L

/* Negative return codes of OWX_GATE_VFS_READ (must match OpenWindows/
 * inc/ow_syscall.h).  A non-negative return is the byte count copied. */
#define OWX_GATE_VFS_EINVAL (-2)   /* NULL path/buffer, or zero capacity      */
#define OWX_GATE_VFS_EFAULT (-3)   /* pointer not in this process's memory    */
#define OWX_GATE_VFS_EACCES (-4)   /* OWFS permission denied (mode)           */
#define OWX_GATE_VFS_ENOENT (-5)   /* no such file, or hidden from caller     */
#define OWX_GATE_VFS_EIO    (-6)   /* backend I/O or a corrupt file           */

/* The trap, and the only place in this header that names a register.
 *
 * R8 is pinned with a fixed-register constraint rather than a plain "r": the
 * ABI says argument 3 travels in R8, and an ordinary constraint would let the
 * compiler pick RAX or RCX and silently clobber an argument.  The kernel's trap
 * frame preserves R8, so the local can be reused across calls. */
static inline int64_t owx_gate_call(int64_t id, int64_t p1, int64_t p2,
                                    int64_t p3) {
    register int64_t p3_r8 __asm__("r8") = p3;
    int64_t ret;
    __asm__ __volatile__ ("int $0x80"
                          : "=a"(ret)
                          : "a"(id), "c"(p1), "d"(p2), "r"(p3_r8)
                          : "memory", "cc");
    return ret;
}

/* ---- Console output ------------------------------------------------------- */

/* The entire console API is one call taking a NUL-terminated string in this
 * process's own memory.  Returns 1, or -1 if the pointer was NULL. */
static inline int owx_puts(const char* s) {
    return (int)owx_gate_call(OWX_GATE_UART_WRITE, (int64_t)(uintptr_t)s, 0, 0);
}

static inline void owx_putc(char c) {
    char one[2];
    one[0] = c;
    one[1] = '\0';
    (void)owx_puts(one);
}

/* Unsigned decimal, right way up.  Digits are produced least-significant first
 * and flushed reversed, so no buffer size has to be guessed for the caller. */
static inline void owx_put_u64(uint64_t v) {
    char tmp[20];
    uint32_t n = 0u;
    if (v == 0u) { owx_putc('0'); return; }
    while (v > 0u && n < (uint32_t)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (int)(v % 10u));
        v /= 10u;
    }
    while (n > 0u) {
        owx_putc(tmp[--n]);
    }
}

static inline void owx_put_i64(int64_t v) {
    if (v < 0) {
        owx_putc('-');
        /* Negate in unsigned space so INT64_MIN does not overflow. */
        owx_put_u64((uint64_t)0u - (uint64_t)v);
    } else {
        owx_put_u64((uint64_t)v);
    }
}

/* ---- Process services ----------------------------------------------------- */

static inline int64_t owx_getpid(void) {
    return owx_gate_call(OWX_GATE_PS_GET_PID, 0, 0, 0);
}

static inline void owx_yield(void) {
    (void)owx_gate_call(OWX_GATE_PS_YIELD, 0, 0, 0);
}

/* Blocking sleep, in scheduler ticks (100 Hz). */
static inline void owx_sleep(int64_t ticks) {
    (void)owx_gate_call(OWX_GATE_PS_SLEEP, ticks, 0, 0);
}

static inline void owx_exit(int64_t code) {
    (void)owx_gate_call(OWX_GATE_PS_EXIT, code, 0, 0);
}

/* ---- Console input -------------------------------------------------------- */

/* Non-blocking by contract: 0..255, or -1 when nothing is waiting. */
static inline int owx_getc(void) {
    return (int)owx_gate_call(OWX_GATE_UART_READ, 0, 0, 0);
}

static inline int owx_getc_wait(void) {
    for (;;) {
        int c = owx_getc();
        if (c >= 0) return c;
        owx_sleep(1);
    }
}

/* ---- Loading another image ------------------------------------------------ */

/* Spawn a named OWX1 image from the primary OWFS volume as a new process.
 * Returns the new PID, or -1. */
static inline int64_t owx_spawn(const char* image) {
    return owx_gate_call(OWX_GATE_PS_SPAWN_OWX,
                         (int64_t)(uintptr_t)image, 0, 0);
}

/* Spawn with an explicit argument vector.  `argv` is a NULL-terminated array of
 * NUL-terminated strings in this process's memory; the kernel copies them into
 * the child before it runs, so they need not outlive the call.  Passing NULL is
 * exactly owx_spawn(): the child's argv[0] defaults to its process name. */
static inline int64_t owx_spawnv(const char* image, const char* const* argv) {
    return owx_gate_call(OWX_GATE_PS_SPAWN_OWX,
                         (int64_t)(uintptr_t)image,
                         (int64_t)(uintptr_t)argv, 0);
}

/* ---- Reading a file from the primary OWFS volume --------------------------
 *
 * `path` is a NUL-terminated name in this process's own memory and `buf` is a
 * writable buffer in this process's own memory; the kernel validates both
 * against this process's address space before it touches either.  Returns the
 * number of bytes copied (0 for an empty file), or one of the negative
 * OWX_GATE_VFS_E* codes above.
 *
 * Access is enforced by the OWFS permission model against this process's
 * identity, not the kernel's: a file whose mode or OWFS_SEC_HIDDEN flag denies
 * the caller is reported as EACCES (mode) or ENOENT (hidden), exactly as a
 * Ring 0 reader would see it. */
static inline int64_t owx_file_read(const char* path, void* buf,
                                    uint32_t cap) {
    return owx_gate_call(OWX_GATE_VFS_READ, (int64_t)(uintptr_t)path,
                         (int64_t)(uintptr_t)buf, (int64_t)cap);
}

#endif /* OWX_GATE_H */
