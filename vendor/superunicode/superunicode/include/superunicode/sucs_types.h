#ifndef SUPERUNICODE_SUCS_TYPES_H
#define SUPERUNICODE_SUCS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* 31-bit code point representation (guarded so the identical typedef can
 * safely coexist with sutf/include/sucs_types.h and extsucs_types.h in a
 * single translation unit). */
#ifndef SUCS_CHAR_T_DEFINED
#define SUCS_CHAR_T_DEFINED
typedef uint32_t sucs_char_t;
#endif

/* Maximum valid SUCS code point boundary (31-bit maximum).
 * Guarded so identical constants can coexist with sutf/sucs_types.h and
 * extsucs_types.h in a single translation unit. */
#ifndef SUCS_MAX_CODEPOINT
#define SUCS_MAX_CODEPOINT 0x7FFFFFFFUL
#endif

/* System Control Plane (SCP) boundaries: Zone 0, District 17 (0x11).
 * Guarded so identical constants can coexist with extsucs_types.h in a
 * single translation unit. */
#ifndef SUCS_SCP_MIN
#define SUCS_SCP_MIN 0x00110000UL
#endif
#ifndef SUCS_SCP_MAX
#define SUCS_SCP_MAX 0x0011FFFFUL
#endif

/* Native SUCS Formatting & System Control Points (SCP) */
#define SUCS_FMT_BOLD_ON    0x00110000UL
#define SUCS_FMT_BOLD_OFF   0x00110001UL
#define SUCS_FMT_ITALIC_ON  0x00110002UL
#define SUCS_FMT_ITALIC_OFF 0x00110003UL
#define SUCS_FMT_COLOR_RGB  0x00110010UL
#define SUCS_FMT_RESET      0x001100FFUL

/* Kernel Security Trap Range & Sentinel (31-bit Base SUCS).
 * Guarded so identical constants can coexist with sutf/sucs_types.h and
 * extsucs_types.h in a single translation unit. */
#ifndef SUCS_KERNEL_TRAP_MIN
#define SUCS_KERNEL_TRAP_MIN   0x7FFFFFF0UL
#endif
#ifndef SUCS_KERNEL_TRAP_MAX
#define SUCS_KERNEL_TRAP_MAX   0x7FFFFFFEUL
#endif
#ifndef SUCS_INVALID_CODEPOINT
#define SUCS_INVALID_CODEPOINT 0x7FFFFFFFUL
#endif

/* Base SUCS codepoint validator — guarded so the identical inline in
 * sutf/sucs_types.h can coexist in a single translation unit.
 * Rejects: values beyond the 31-bit space, the Kernel Security Trap range,
 * and the in-band sentinel. */
#ifndef SUCS_SUCS_IS_VALID_DEFINED
#define SUCS_SUCS_IS_VALID_DEFINED
static inline bool sucs_is_valid(sucs_char_t cp) {
    if (cp > SUCS_MAX_CODEPOINT) {
        return false;
    }
    if (cp >= SUCS_KERNEL_TRAP_MIN && cp <= SUCS_KERNEL_TRAP_MAX) {
        return false;
    }
    if (cp == SUCS_INVALID_CODEPOINT) {
        return false;
    }
    return true;
}
#endif

/* BANcode Registry Plugin Range: Kernel Damage Control registry (inside SCP).
 * Guarded so identical constants can coexist with extsucs_types.h in a
 * single translation unit. */
#ifndef SUCS_BANCODE_REGISTRY_MIN
#define SUCS_BANCODE_REGISTRY_MIN 0x0011A000UL
#endif
#ifndef SUCS_BANCODE_REGISTRY_MAX
#define SUCS_BANCODE_REGISTRY_MAX 0x0011AEFFUL
#endif

/* B+ BANcode: 2048 codepoints (0x0011A000-0x0011A7FF).
 * Unrecoverable critical panics, hardware faults, fatal exceptions.
 * All slots unassigned (v0.1.0). */
#ifndef SUCS_BANCODE_RANGE_MIN
#define SUCS_BANCODE_RANGE_MIN 0x0011A000UL
#endif
#ifndef SUCS_BANCODE_RANGE_MAX
#define SUCS_BANCODE_RANGE_MAX 0x0011A7FFUL
#endif

/* W+ WARNcode: 1024 codepoints (0x0011A800-0x0011ABFF).
 * Non-fatal telemetry; threshold alerts, degraded performance, predictive maintenance.
 * All slots unassigned (v0.1.0). */
#ifndef SUCS_WARNCODE_RANGE_MIN
#define SUCS_WARNCODE_RANGE_MIN 0x0011A800UL
#endif
#ifndef SUCS_WARNCODE_RANGE_MAX
#define SUCS_WARNCODE_RANGE_MAX 0x0011ABFFUL
#endif

/* C+ COMcode: 512 codepoints (0x0011AC00-0x0011ADFF).
 * IPC events, bus orchestration, hardware handshakes, driver attachment,
 * control plane signals; also success reports.
 * All slots unassigned (v0.1.0). */
#ifndef SUCS_COMCODE_RANGE_MIN
#define SUCS_COMCODE_RANGE_MIN 0x0011AC00UL
#endif
#ifndef SUCS_COMCODE_RANGE_MAX
#define SUCS_COMCODE_RANGE_MAX 0x0011ADFFUL
#endif

/* S+ SOFTcode: 256 codepoints (0x0011AE00-0x0011AEFF).
 * Soft recovery, fault containment, cache reconciliation, self-healing routines;
 * also soft errors. All slots unassigned (v0.1.0). */
#ifndef SUCS_SOFTCODE_RANGE_MIN
#define SUCS_SOFTCODE_RANGE_MIN 0x0011AE00UL
#endif
#ifndef SUCS_SOFTCODE_RANGE_MAX
#define SUCS_SOFTCODE_RANGE_MAX 0x0011AEFFUL
#endif

/* Kernel Security Trap Damage Control Dispatch geometry */
#define SUCS_TRAP_SLOT_COUNT     15
#define SUCS_BANCODES_PER_TRAP   128

/* SUES Status Return Codes */
typedef enum {
    SUES_SUCCESS               = 0,
    SUES_ERR_INVALID_BYTE      = -1,
    SUES_ERR_BUFFER_TOO_SMALL  = -2,
    SUES_ERR_OUT_OF_BOUNDS     = -3,
    SUES_ERR_INVALID_CODEPOINT = -4  /* Sentinel or Kernel Security Trap range */
} sues_status_t;

/* Code Point Classification Types */
typedef enum {
    SUCS_TYPE_UNICODE_COMPAT = 0, /* 0x00000000 - 0x0010FFFF: Unicode Parity Zone */
    SUCS_TYPE_SYS_FUNCTION   = 1, /* 0x00110000 - 0x0011FFFF: System Control Plane (SCP) */
    SUCS_TYPE_NATIVE_ALLOC   = 2, /* 0x00120000 - 0x7FFFFFFF: Native Extended Allocations */
    SUCS_TYPE_INVALID        = 3  /* Out of the 31-bit Base SUCS address space */
} sucs_codepoint_type_t;

/* BANcode Registry Classification Types.
 * Guarded so the identical enum can coexist with extsucs_types.h in a
 * single translation unit. */
#ifndef SUCS_BANCODE_TYPE_T_DEFINED
#define SUCS_BANCODE_TYPE_T_DEFINED
typedef enum {
    SUCS_BANCODE_NONE  = 0, /* Not in the BANcode Registry */
    SUCS_BANCODE_FATAL = 1, /* B+ 0x0011A000-0x0011A7FF: Unrecoverable critical panics, hardware faults, fatal exceptions */
    SUCS_BANCODE_WARN  = 2, /* W+ 0x0011A800-0x0011ABFF: Non-fatal telemetry; threshold alerts, degraded performance, predictive maintenance */
    SUCS_BANCODE_COM   = 3, /* C+ 0x0011AC00-0x0011ADFF: IPC events, bus orchestration, hardware handshakes, driver attachment, control plane signals; also success reports */
    SUCS_BANCODE_SOFT  = 4  /* S+ 0x0011AE00-0x0011AEFF: Soft recovery, fault containment, cache reconciliation, self-healing routines; also soft errors */
} sucs_bancode_type_t;
#endif

/* Kernel-Safe Descriptor */
typedef struct {
    uint32_t length_bytes;
    uint32_t capacity_bytes;
    char*    buffer;
} SUCS_STRING;

#endif /* SUPERUNICODE_SUCS_TYPES_H */
