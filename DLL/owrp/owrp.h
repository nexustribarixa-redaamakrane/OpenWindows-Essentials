/*
 * owrp.h - OpenWindows Ring Portal (.owd)
 *
 * System call gate library for crossing privilege boundaries on x86_64.
 * Provides owrp_invoke_gate() which performs a raw syscall/int $0x80
 * transition, mapping user-space arguments into kernel registers.
 *
 * Conforms to OWD1 binary format (Extensions/owd_format.h).
 * C99 freestanding - no heap, no hosted libc.
 */

#ifndef OWRP_H
#define OWRP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  OWD1 Binary Header Reference                                       */
/*  (magic 0x4F574431, header 192 bytes, see owd_format.h)            */
/* ------------------------------------------------------------------ */

#define OWRP_LIB_TYPE           OWD_LIBTYPE_HYBRID   /* 0x02 user+kernel */
#define OWRP_TARGET_ARCH        0x02u                /* x86_64           */
#define OWRP_ALIGNMENT_LOG2     12u                  /* 4 KiB pages      */

/* ------------------------------------------------------------------ */
/*  System Call Numbers                                                */
/* ------------------------------------------------------------------ */

/* -- Kernel / System ------------------------------------------------ */
#define OWRP_SYS_NOP            0x000u
#define OWRP_SYS_HALT           0x001u  /* Emergency halt              */
#define OWRP_SYS_REBOOT         0x002u  /* Emergency reboot             */
#define OWRP_SYS_GET_TSC        0x003u  /* Read timestamp counter       */
#define OWRP_SYS_GET_CPUID      0x004u  /* CPUID leaf/arg               */

/* -- Memory --------------------------------------------------------- */
#define OWRP_SYS_MEM_REQUEST    0x010u  /* Request memory block         */
#define OWRP_SYS_MEM_RELEASE    0x011u  /* Release memory block         */
#define OWRP_SYS_MEM_QUERY      0x012u  /* Query memory stats           */

/* -- I/O ------------------------------------------------------------ */
#define OWRP_SYS_IO_READ        0x020u  /* Port I/O read (inb/inw/inl)  */
#define OWRP_SYS_IO_WRITE       0x021u  /* Port I/O write (outb/outw/outl) */
#define OWRP_SYS_IRQ_MASK       0x022u  /* Mask/unmask IRQ              */

/* -- BANcode / Exception -------------------------------------------- */
#define OWRP_SYS_BAN_RAISE      0x030u  /* Raise a BANcode              */
#define OWRP_SYS_BAN_QUERY      0x031u  /* Query last BANcode state     */

/* -- Telemetry ------------------------------------------------------ */
#define OWRP_SYS_TELEMETRY      0x040u  /* Write telemetry record       */

/* -- Timer ---------------------------------------------------------- */
#define OWRP_SYS_TIMER_READ     0x050u  /* Read monotonic timer         */
#define OWRP_SYS_TIMER_SET      0x051u  /* Set one-shot timer           */

/* -- Display -------------------------------------------------------- */
#define OWRP_SYS_FB_INFO        0x060u  /* Query framebuffer info       */
#define OWRP_SYS_FB_PUTPIXEL    0x061u  /* Write pixel to framebuffer   */

/* -- Serial console (sio.owc) --------------------------------------- */
#define OWRP_SYS_SIO_WRITE      0x070u  /* Ring-3 SUTF-8 write, serial  */
#define OWRP_SYS_SIO_READ       0x071u  /* Poll received byte           */
#define OWRP_SYS_SIO_IDLE       0x072u  /* Query TX idle / line status  */

/* ------------------------------------------------------------------ */
/*  Status Codes — BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A77F): Fatal gate faults                     */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal warnings                    */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
/* ------------------------------------------------------------------ */

typedef uint32_t owrp_status_t;

#define OWRP_OK                         0x00000000u  /* Success              */
/* B+ Fatal */
#define OWRP_ERR_INVALID_SYSCALL        0x0011A000u  /* Unknown syscall nr   */
#define OWRP_ERR_PRIVILEGE              0x0011A001u  /* Ring level denied    */
#define OWRP_ERR_INVALID_ARG            0x0011A002u  /* Bad gate argument    */
#define OWRP_ERR_UNMAPPED               0x0011A003u  /* Gate not registered  */
#define OWRP_ERR_DENIED                 0x0011A004u  /* Access policy denied */
/* W+ Warning */
#define OWRP_ERR_TIMEOUT                0x0011A800u  /* Gate call timed out  */
/* S+ Soft */
#define OWRP_ERR_FAULT                  0x0011AE00u  /* Transient gate fault */

/* ------------------------------------------------------------------ */
/*  Gate Return Value                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    int64_t     value;         /* Primary return value (RAX)          */
    int64_t     error;         /* Error code (RDX), 0 = success       */
    uint64_t    r8;            /* Secondary return (R8, if needed)    */
    uint64_t    r9;            /* Tertiary return  (R9, if needed)    */
} owrp_gate_result_t;

/* ------------------------------------------------------------------ */
/*  Gate Table Entry                                                   */
/* ------------------------------------------------------------------ */

#define OWRP_GATE_NAME_MAX  32u

typedef struct {
    uint32_t    syscall_nr;        /* System call number              */
    uint16_t    required_ring;     /* Minimum ring level (0=kernel)   */
    uint8_t     param_count;       /* Expected parameter count        */
    uint8_t     flags;             /* Gate flags                      */
    char        name[OWRP_GATE_NAME_MAX];
} owrp_gate_entry_t;

#define OWRP_GATE_FLAG_ACTIVE    0x01u
#define OWRP_GATE_FLAG_KERNEL    0x02u  /* Kernel-only gate             */
#define OWRP_GATE_FLAG_BOOT      0x04u  /* Available during boot phase  */

/* ------------------------------------------------------------------ */
/*  OWRP Context                                                       */
/* ------------------------------------------------------------------ */

#define OWRP_MAX_GATES      128u
#define OWRP_CALL_LOG_SIZE  32u

typedef struct {
    owrp_gate_entry_t   gates[OWRP_MAX_GATES];
    uint32_t            gate_count;

    /* Call trace log (circular) */
    struct {
        uint64_t    timestamp;
        uint32_t    syscall_nr;
        int64_t     result;
        int64_t     error;
    } call_log[OWRP_CALL_LOG_SIZE];
    uint32_t            log_head;
    uint32_t            log_count;

    /* Statistics */
    uint64_t            total_calls;
    uint64_t            total_errors;
    uint64_t            total_privilege_denials;

    bool                initialized;
    uint8_t             current_ring;    /* Current execution ring      */
    uint8_t             boot_phase;      /* 1 = still in boot phase     */
    uint8_t             reserved[5];
} owrp_context_t;

/* ------------------------------------------------------------------ */
/*  Core Gate API                                                      */
/* ------------------------------------------------------------------ */

/*
 * Initialize the ring portal context.
 * Zeros all gates, sets current_ring = 3 (user mode default).
 */
owrp_status_t owrp_init(owrp_context_t *ctx);

/*
 * Invoke a system call through the gate.
 *
 * On x86_64, performs:
 *   RAX = sys_nr
 *   RDI = arg1
 *   RSI = arg2
 *   RDX = arg3
 *   syscall
 *   RAX -> result.value
 *   RDX -> result.error
 *
 * Returns the gate result. error != 0 on failure.
 */
owrp_gate_result_t owrp_invoke_gate(
    owrp_context_t *ctx,
    uint64_t        sys_nr,
    uint64_t        arg1,
    uint64_t        arg2,
    uint64_t        arg3
);

/*
 * Invoke with all 6 user-visible register args.
 *   RAX = sys_nr, RDI = a1, RSI = a2, RDX = a3,
 *   R10 = a4,     R8  = a5, R9  = a6
 */
owrp_gate_result_t owrp_invoke_gate_full(
    owrp_context_t *ctx,
    uint64_t        sys_nr,
    uint64_t        arg1,
    uint64_t        arg2,
    uint64_t        arg3,
    uint64_t        arg4,
    uint64_t        arg5,
    uint64_t        arg6
);

/* ------------------------------------------------------------------ */
/*  Gate Registration                                                  */
/* ------------------------------------------------------------------ */

/*
 * Register a system call in the local gate table.
 * Used by library init to declare which syscalls this .owd uses.
 */
owrp_status_t owrp_register_gate(
    owrp_context_t         *ctx,
    uint32_t                syscall_nr,
    const char             *name,
    uint8_t                 param_count,
    uint16_t                required_ring,
    uint8_t                 flags
);

/*
 * Look up a gate entry by syscall number.
 * Returns NULL if not registered.
 */
const owrp_gate_entry_t *owrp_find_gate(
    const owrp_context_t *ctx,
    uint32_t              syscall_nr
);

/* ------------------------------------------------------------------ */
/*  Ring Level Management                                              */
/* ------------------------------------------------------------------ */

/*
 * Transition to a new ring level. Only kernel (ring 0) can set
 * arbitrary levels. User (ring 3) can only enter kernel via gates.
 */
owrp_status_t owrp_set_ring(owrp_context_t *ctx, uint8_t ring);

/*
 * Enter boot phase (restricts available syscalls to BOOT-flagged gates).
 */
void owrp_enter_boot_phase(owrp_context_t *ctx);

/*
 * Exit boot phase (all gates available).
 */
void owrp_exit_boot_phase(owrp_context_t *ctx);

/* ------------------------------------------------------------------ */
/*  Call Log & Statistics                                              */
/* ------------------------------------------------------------------ */

/*
 * Get the last N call log entries. Returns count actually copied.
 */
uint32_t owrp_get_call_log(
    const owrp_context_t *ctx,
    uint32_t             *out_syscall_nrs,
    int64_t              *out_results,
    uint32_t              max_entries
);

/*
 * Get cumulative statistics.
 */
void owrp_get_stats(
    const owrp_context_t *ctx,
    uint64_t *total_calls,
    uint64_t *total_errors,
    uint64_t *total_privilege_denials
);

/*
 * Reset statistics and call log.
 */
void owrp_reset_stats(owrp_context_t *ctx);

#endif /* OWRP_H */
