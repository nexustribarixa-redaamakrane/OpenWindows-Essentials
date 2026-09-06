/*
 * banhammer.h - OpenWindows Banhammer Driver
 *
 * Kernel panic handler and emergency execution engine. Processes
 * panic severity, validates configuration, captures CPU state,
 * writes telemetry to boot storage, and executes the emergency
 * halt loop (CLI/HLT on x86_64).
 *
 * Cross-references:
 *   BANcode (Documents/BANcode): bancode_t, trap range 0x7FFFFFF0-0x7FFFFFFE
 *   Sentinel (Extensions/sentinel_format.h): sentinel recovery hooks
 *   BootVID (DLL/bootvid/): panic screen rendering
 *   OpenWindows-Storage: telemetry LBA write target
 *   VIP: sector addressing (512 bytes/sector)
 *
 * C99 freestanding - no dynamic allocation.
 */

#ifndef BANHAMMER_H
#define BANHAMMER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "banhammer_config.h"

/* ------------------------------------------------------------------ */
/*  Compiler Portability                                               */
/* ------------------------------------------------------------------ */

#if defined(__GNUC__) || defined(__clang__)
#define BH_NORETURN     __attribute__((noreturn))
#define BH_PACKED       __attribute__((packed))
#define BH_INLINE       static inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define BH_NORETURN     __declspec(noreturn)
#define BH_PACKED
#define BH_INLINE       static __forceinline
#else
#define BH_NORETURN
#define BH_PACKED
#define BH_INLINE       static inline
#endif

/* ------------------------------------------------------------------ */
/*  BANcode Constants (from Documents/BANcode/bancode_all.h)          */
/* ------------------------------------------------------------------ */

typedef uint32_t bancode_t;

#define BH_BANCODE_START        0x0011A000u  /* B+ range start           */
#define BH_BANCODE_END          0x0011A7FFu  /* B+ range end             */
#define BH_TRAP_MIN             0x7FFFFFF0u  /* Trap table start         */
#define BH_TRAP_MAX             0x7FFFFFFEu  /* Trap table end           */
#define BH_TRAP_SLOT_COUNT      15u
#define BH_BANCODES_PER_TRAP    128u
#define BH_INVALID_CODEPOINT    0x7FFFFFFFu

/* ------------------------------------------------------------------ */
/*  Driver Version                                                     */
/* ------------------------------------------------------------------ */

#define BH_VERSION_MAJOR    1u
#define BH_VERSION_MINOR    0u
#define BH_VERSION_PATCH    0u

/* ------------------------------------------------------------------ */
/*  Telemetry Ring Buffer                                              */
/* ------------------------------------------------------------------ */

#define BH_TELEMETRY_RING_SIZE  64u
#define BH_TELEMETRY_MSG_MAX    128u

/* ------------------------------------------------------------------ */
/*  CPU State Capture (x86_64)                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    /* General-purpose registers (captured at panic) */
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t rsp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;

    /* Instruction pointer and flags */
    uint64_t rip;
    uint64_t rflags;

    /* Control registers */
    uint64_t cr0;
    uint64_t cr2;    /* Faulting address */
    uint64_t cr3;    /* Page table base */
    uint64_t cr4;

    /* Segment registers (zero-extended) */
    uint16_t cs;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint16_t ss;

    /* Model-Specific Registers */
    uint64_t msr_star;      /* SYSCALL entry CS/SS */
    uint64_t msr_lstar;     /* SYSCALL entry RIP */
    uint64_t msr_fmask;     /* SYSCALL RFLAGS mask */

    /* Timestamp counter at panic */
    uint64_t tsc;

    /* Padding to 256-byte boundary */
    uint8_t  reserved[32];
} banhammer_cpu_state_t;

/* ------------------------------------------------------------------ */
/*  Telemetry Record                                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t timestamp;            /* TSC at time of record           */
    uint32_t bancode;              /* Associated BANcode              */
    uint8_t  severity;             /* BHCF_SEVERITY_* level           */
    uint8_t  behavior_flags;       /* Active BHCF_FLAG_* at panic     */
    uint16_t message_offset;       /* Offset into string area         */
    uint32_t cpu_rip;              /* Truncated instruction pointer    */
    uint32_t cpu_cr2;              /* Truncated faulting address      */
    uint8_t  trap_slot;            /* Trap slot index (0-14, or 0xFF)  */
    uint8_t  reserved[7];          /* Alignment padding               */
} banhammer_telemetry_record_t;

/* ------------------------------------------------------------------ */
/*  Banhammer Status Codes — BANcode mapped                            */
/*  B+ (0x0011A000-0x0011A77F): Fatal faults                          */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal warnings                    */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
/* ------------------------------------------------------------------ */

typedef uint32_t banhammer_status_t;

#define BANHAMMER_OK                      0x00000000u  /* Success              */
/* B+ Fatal */
#define BANHAMMER_ERR_INVALID_CONFIG      0x0011A000u  /* Config invalid       */
#define BANHAMMER_ERR_TELEMETRY_WRITE     0x0011A001u  /* Telemetry I/O fail   */
#define BANHAMMER_ERR_CPU_CAPTURE_FAILED  0x0011A002u  /* CPU capture failed   */
#define BANHAMMER_ERR_REBOOT_FAILED       0x0011A003u  /* Reboot escalation    */
#define BANHAMMER_ERR_DUMP_FAILED         0x0011A004u  /* Memory dump failed   */
/* W+ Warning */
#define BANHAMMER_ERR_SEVERITY_LOW        0x0011A800u  /* Below threshold      */
/* S+ Soft */
#define BANHAMMER_ERR_TELEMETRY_FULL      0x0011AE00u  /* Ring buffer full     */
#define BANHAMMER_ERR_NO_CONFIG           0x0011AE01u  /* No config attached   */

/* ------------------------------------------------------------------ */
/*  Driver Context                                                     */
/* ------------------------------------------------------------------ */

typedef struct {
    bhcf_config_t              *config;          /* Active config       */
    banhammer_cpu_state_t              cpu_state;       /* Captured CPU state  */
    banhammer_telemetry_record_t       ring[BH_TELEMETRY_RING_SIZE];
    char                        string_pool[BH_TELEMETRY_RING_SIZE * BH_TELEMETRY_MSG_MAX];
    uint32_t                    ring_head;
    uint32_t                    ring_count;
    uint32_t                    string_used;
    uint32_t                    total_panics;
    uint32_t                    total_strikes;
    uint32_t                    total_reboots;
    uint32_t                    total_dumps;
    uint32_t                    total_telemetry_writes;
    bool                        initialized;
    uint8_t                     last_severity;
    uint32_t                    last_bancode;
    uint64_t                    last_timestamp;
} banhammer_context_t;

/* ------------------------------------------------------------------ */
/*  Driver API                                                         */
/* ------------------------------------------------------------------ */

/*
 * Initialize the Banhammer driver context.
 * Stores the config pointer (caller must keep it alive).
 * Zeros all telemetry and CPU state.
 * Returns BANHAMMER_OK on success.
 */
banhammer_status_t banhammer_init(banhammer_context_t *ctx, bhcf_config_t *config);

/*
 * Release driver context. Zeros all state.
 */
void banhammer_release(banhammer_context_t *ctx);

/*
 * CAPTURE CPU STATE
 * Fills ctx->cpu_state with the provided register values.
 * On x86_64, this is called from the panic handler with
 * registers saved on the stack before the call.
 * Returns BANHAMMER_OK.
 */
banhammer_status_t banhammer_capture_cpu(
    banhammer_context_t *ctx,
    uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx,
    uint64_t rsi, uint64_t rdi, uint64_t rbp, uint64_t rsp,
    uint64_t r8,  uint64_t r9,  uint64_t r10, uint64_t r11,
    uint64_t r12, uint64_t r13, uint64_t r14, uint64_t r15,
    uint64_t rip, uint64_t rflags,
    uint64_t cr0, uint64_t cr2, uint64_t cr3, uint64_t cr4,
    uint16_t cs,  uint16_t ds,  uint16_t es,  uint16_t fs,
    uint16_t gs,  uint16_t ss
);

/*
 * STRIKE - Main panic processing entry point.
 * `message` is a NUL-terminated SUTF-8 transport string (ASCII-compatible;
 * non-printable SUCS codepoints are tolerated by the telemetry writer).
 *
 *   1. Validates config and severity threshold
 *   2. Records the BANcode and severity
 *   3. Writes telemetry record to ring buffer
 *   4. If BHCF_FLAG_TELEMETRY_ON: flushes ring to boot storage
 *   5. If BHCF_FLAG_MEMORY_DUMP: dumps registered regions to storage
 *   6. If BHCF_FLAG_AUTO_REBOOT: reboots after delay
 *   7. Otherwise: enters emergency HLT loop
 *
 * Returns BANHAMMER_OK if the system survived (severity below threshold).
 * Does not return on BHCF_SEVERITY_CATASTROPHIC.
 */
banhammer_status_t banhammer_strike(
    banhammer_context_t *ctx,
    bancode_t    bancode,
    uint8_t      severity,
    const char  *message
);

/*
 * Write a single telemetry record to the ring buffer.
 * Copies the message string into the string pool.
 * Returns BANHAMMER_OK or BANHAMMER_ERR_TELEMETRY_FULL.
 */
banhammer_status_t banhammer_telemetry_write(
    banhammer_context_t *ctx,
    bancode_t     bancode,
    uint8_t       severity,
    const char   *message,
    uint64_t      timestamp
);

/*
 * Flush the telemetry ring to boot storage via HTL block I/O.
 * Writes records to the LBA/sector region specified in config.
 * Returns BANHAMMER_OK or BANHAMMER_ERR_TELEMETRY_WRITE.
 */
banhammer_status_t banhammer_telemetry_flush(banhammer_context_t *ctx);

/*
 * Get the most recent telemetry record (NULL if none).
 */
const banhammer_telemetry_record_t *banhammer_telemetry_last(const banhammer_context_t *ctx);

/*
 * Get telemetry record count in ring.
 */
uint32_t banhammer_telemetry_count(const banhammer_context_t *ctx);

/*
 * EMERGENCY HALT LOOP
 * Disables interrupts (CLI) and halts (HLT) in an infinite loop.
 * On x86_64 this is: asm volatile("cli\n1: hlt\njmp 1b").
 * This function DOES NOT RETURN.
 */
void BH_NORETURN banhammer_emergency_halt(void);

/*
 * EMERGENCY REBOOT
 * Triggers a CPU triple-fault or keyboard controller reset.
 * On x86_64 this writes to port 0x64 (keyboard controller reset)
 * or loads an invalid IDT and triggers int3.
 * This function DOES NOT RETURN.
 */
void BH_NORETURN banhammer_emergency_reboot(void);

/*
 * MEMORY DUMP
 * Writes the registered dump regions to storage at the telemetry LBA.
 * Returns BANHAMMER_OK or BANHAMMER_ERR_DUMP_FAILED.
 */
banhammer_status_t banhammer_memory_dump(banhammer_context_t *ctx);

/*
 * Resolve a BANcode to its trap slot index.
 * Returns the slot (0-14) or 0xFF if unmapped.
 */
uint8_t banhammer_bancode_to_trap_slot(bancode_t bancode);

/*
 * Get the BANcode name string (from BANcode registry).
 * Falls back to "UNKNOWN" if not in registry.
 */
const char *banhammer_bancode_name(bancode_t bancode);

/*
 * Get driver statistics.
 */
void banhammer_get_stats(
    const banhammer_context_t *ctx,
    uint32_t *total_panics,
    uint32_t *total_strikes,
    uint32_t *total_reboots,
    uint32_t *total_dumps
);

#endif /* BANHAMMER_H */
