#ifndef EXTSUCS_TYPES_H
#define EXTSUCS_TYPES_H

/**
 * ExtSUCS (SuperUnicode Extended) Character Encoding Specification
 *
 * ExtSUCS is strictly a CHARACTER ENCODING defining an abstract,
 * unbounded codepoint numerical address space (0 -> infinity).
 * The encoding itself has NO upper limit — it is conceptually infinite.
 *
 * IMPLEMENTATION NOTE: The current C99 implementation uses uint64_t as a
 * finite container, which can address codepoints 0 through 2^64-1. This
 * does NOT define the encoding's boundary — it is merely the widest
 * integer type available in this implementation. Future implementations
 * may use wider types (uint128_t, arbitrary-precision integers, etc.)
 * to address higher regions of the infinite ExtSUCS encoding space.
 *
 * Within the current 64-bit container, ALL values are valid codepoint
 * addresses with the sole exception of the inherited Base SUCS Kernel
 * Security Trap Range (0x7FFFFFF0 - 0x7FFFFFFE).
 *
 * ExtSUCS also inherits the System Control Plane (SCP) boundaries
 * (0x00110000 - 0x0011FFFF) and, inside it, the BANcode Registry Plugin
 * Range (0x0011A000 - 0x0011AEFF) reserved identically to Base SUCS.
 * SCP and BANcode addresses remain VALID codepoints (system function /
 * damage-control registry regions) — only the trap range is rejected.
 *
 * IMPORTANT: 0x7FFFFFFF (SUCS_INVALID_CODEPOINT) is the boundary sentinel
 * of the 31-bit Base SUCS encoding only. In ExtSUCS (0 -> infinity), the
 * value 0x7FFFFFFF IS a valid codepoint address — ExtSUCS has no upper
 * boundary and therefore no in-band sentinel of any kind.
 *
 * Error handling is strictly OUT-OF-BAND: functions return bool/size_t
 * success indicators and populate decoded codepoints via output pointers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================================================
 * ExtSUCS Character Encoding Type (64-bit implementation of unbounded space)
 * ============================================================================ */
typedef uint64_t sucs_ex_char_t;

/* ============================================================================
 * Base SUCS Character Encoding Type (31-bit bounded space, inherited)
 *
 * Guarded so the identical typedef can coexist with superunicode/sucs_types.h
 * and sutf/sucs_types.h in a single translation unit.
 * ============================================================================ */
#ifndef SUCS_CHAR_T_DEFINED
#define SUCS_CHAR_T_DEFINED
typedef uint32_t sucs_char_t;
#endif

/* ============================================================================
 * Base SUCS Character Encoding Sentinels & Boundaries
 *
 * These constants define the 31-bit Base SUCS bounded encoding:
 * - SUCS_INVALID_CODEPOINT (0x7FFFFFFF): Sentinel for Base SUCS ONLY.
 *   This value IS a valid ExtSUCS codepoint (ExtSUCS is unbounded).
 * - SUCS_TRAP_RANGE: Inherited by ExtSUCS — reserved across BOTH encodings.
 * Guarded so identical constants can coexist with superunicode/sucs_types.h
 * and sutf/sucs_types.h in a single translation unit. */
#ifndef SUCS_INVALID_CODEPOINT
#define SUCS_INVALID_CODEPOINT  ((sucs_char_t)0x7FFFFFFFUL)
#endif
#ifndef SUCS_MAX_CODEPOINT
#define SUCS_MAX_CODEPOINT      ((sucs_char_t)0x7FFFFFFFUL)
#endif
#ifndef SUCS_TRAP_RANGE_MIN
#define SUCS_TRAP_RANGE_MIN     ((sucs_char_t)0x7FFFFFF0UL)
#endif
#ifndef SUCS_TRAP_RANGE_MAX
#define SUCS_TRAP_RANGE_MAX     ((sucs_char_t)0x7FFFFFFEUL)
#endif

/* ============================================================================
 * System Control Plane (SCP) & BANcode Registry — inherited by ExtSUCS
 *
 * The System Control Plane (Zone 0, District 0x11) and, inside it, the
 * BANcode Registry Plugin Range (0x0011A000 - 0x0011AEFF) are reserved
 * identically in the unbounded ExtSUCS address space, exactly as in Base
 * SUCS. Guarded so identical constants can safely coexist with
 * superunicode/sucs_types.h in a single translation unit.
 * ============================================================================ */
#ifndef SUCS_SCP_MIN
#define SUCS_SCP_MIN            ((sucs_ex_char_t)0x00110000ULL)
#endif
#ifndef SUCS_SCP_MAX
#define SUCS_SCP_MAX            ((sucs_ex_char_t)0x0011FFFFULL)
#endif

#ifndef SUCS_BANCODE_REGISTRY_MIN
#define SUCS_BANCODE_REGISTRY_MIN ((sucs_ex_char_t)0x0011A000ULL)
#endif
#ifndef SUCS_BANCODE_REGISTRY_MAX
#define SUCS_BANCODE_REGISTRY_MAX ((sucs_ex_char_t)0x0011AEFFULL)
#endif

/* B+ BANcode: 2048 codepoints (0x0011A000-0x0011A7FF).
 * Unrecoverable critical panics, hardware faults, fatal exceptions.
 * All slots unassigned (v0.1.0). */
#ifndef SUCS_BANCODE_RANGE_MIN
#define SUCS_BANCODE_RANGE_MIN  ((sucs_ex_char_t)0x0011A000ULL)
#endif
#ifndef SUCS_BANCODE_RANGE_MAX
#define SUCS_BANCODE_RANGE_MAX  ((sucs_ex_char_t)0x0011A7FFULL)
#endif

/* W+ WARNcode: 1024 codepoints (0x0011A800-0x0011ABFF).
 * Non-fatal telemetry; threshold alerts, degraded performance, predictive maintenance.
 * All slots unassigned (v0.1.0). */
#ifndef SUCS_WARNCODE_RANGE_MIN
#define SUCS_WARNCODE_RANGE_MIN ((sucs_ex_char_t)0x0011A800ULL)
#endif
#ifndef SUCS_WARNCODE_RANGE_MAX
#define SUCS_WARNCODE_RANGE_MAX ((sucs_ex_char_t)0x0011ABFFULL)
#endif

/* C+ COMcode: 512 codepoints (0x0011AC00-0x0011ADFF).
 * IPC events, bus orchestration, hardware handshakes, driver attachment,
 * control plane signals; also success reports.
 * All slots unassigned (v0.1.0). */
#ifndef SUCS_COMCODE_RANGE_MIN
#define SUCS_COMCODE_RANGE_MIN  ((sucs_ex_char_t)0x0011AC00ULL)
#endif
#ifndef SUCS_COMCODE_RANGE_MAX
#define SUCS_COMCODE_RANGE_MAX  ((sucs_ex_char_t)0x0011ADFFULL)
#endif

/* S+ SOFTcode: 256 codepoints (0x0011AE00-0x0011AEFF).
 * Soft recovery, fault containment, cache reconciliation, self-healing routines;
 * also soft errors. All slots unassigned (v0.1.0). */
#ifndef SUCS_SOFTCODE_RANGE_MIN
#define SUCS_SOFTCODE_RANGE_MIN ((sucs_ex_char_t)0x0011AE00ULL)
#endif
#ifndef SUCS_SOFTCODE_RANGE_MAX
#define SUCS_SOFTCODE_RANGE_MAX ((sucs_ex_char_t)0x0011AEFFULL)
#endif

/* BANcode Registry Classification Types (guarded for header coexistence) */
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

/* ============================================================================
 * ExtSUCS Character Encoding Boundary
 *
 * Marks the upper limit of the Base SUCS 31-bit fast-path range within
 * the unbounded ExtSUCS space. NOT a sentinel — just a range marker.
 * ============================================================================ */
#define EXTSUCS_BASE_SUCS_MAX   ((sucs_ex_char_t)0x7FFFFFFFULL)

/* ============================================================================
 * Codepoint Validators
 *
 * extsucs_is_valid(): Validates an ExtSUCS 64-bit codepoint address.
 *   - Returns false ONLY for the inherited Kernel Security Trap Range
 *     (0x7FFFFFF0 - 0x7FFFFFFE). These addresses are reserved hardware
 *     trap slots across BOTH Base SUCS and ExtSUCS.
 *   - Returns true for ALL other 64-bit values, INCLUDING 0x7FFFFFFF
 *     (which is a valid codepoint in the unbounded ExtSUCS space).
 *
 * extsucs_is_base_sucs(): Tests whether an ExtSUCS codepoint falls within
 *   the Base SUCS 31-bit fast-path range (0x00000000 - 0x7FFFFFFF).
 * ============================================================================ */
static inline bool extsucs_is_valid(sucs_ex_char_t ex_cp) {
    /* Inherited Kernel Security Trap Range — reserved across both encodings */
    if (ex_cp >= (sucs_ex_char_t)SUCS_TRAP_RANGE_MIN &&
        ex_cp <= (sucs_ex_char_t)SUCS_TRAP_RANGE_MAX) {
        return false;
    }
    return true;
}

static inline bool extsucs_is_base_sucs(sucs_ex_char_t ex_cp) {
    return ex_cp <= EXTSUCS_BASE_SUCS_MAX;
}

/* ============================================================================
 * System Control Plane (SCP) & BANcode Registry Helpers
 *
 * ExtSUCS 64-bit variants of the inherited Base SUCS classification helpers.
 * SCP codepoints (0x00110000 - 0x0011FFFF) are system function / formatting
 * addresses; the BANcode Registry (0x0011A000 - 0x0011AEFF) is the kernel
 * damage-control registry living inside the SCP.
 * ============================================================================ */
static inline bool extsucs_is_scp_plane(sucs_ex_char_t ex_cp) {
    return (ex_cp >= (sucs_ex_char_t)SUCS_SCP_MIN &&
            ex_cp <= (sucs_ex_char_t)SUCS_SCP_MAX);
}

static inline bool extsucs_is_bancode_registry(sucs_ex_char_t ex_cp) {
    return (ex_cp >= (sucs_ex_char_t)SUCS_BANCODE_REGISTRY_MIN &&
            ex_cp <= (sucs_ex_char_t)SUCS_BANCODE_REGISTRY_MAX);
}

static inline bool extsucs_is_bancode(sucs_ex_char_t ex_cp) {
    return (ex_cp >= (sucs_ex_char_t)SUCS_BANCODE_RANGE_MIN &&
            ex_cp <= (sucs_ex_char_t)SUCS_BANCODE_RANGE_MAX);
}

static inline bool extsucs_is_warncode(sucs_ex_char_t ex_cp) {
    return (ex_cp >= (sucs_ex_char_t)SUCS_WARNCODE_RANGE_MIN &&
            ex_cp <= (sucs_ex_char_t)SUCS_WARNCODE_RANGE_MAX);
}

static inline bool extsucs_is_comcode(sucs_ex_char_t ex_cp) {
    return (ex_cp >= (sucs_ex_char_t)SUCS_COMCODE_RANGE_MIN &&
            ex_cp <= (sucs_ex_char_t)SUCS_COMCODE_RANGE_MAX);
}

static inline bool extsucs_is_softcode(sucs_ex_char_t ex_cp) {
    return (ex_cp >= (sucs_ex_char_t)SUCS_SOFTCODE_RANGE_MIN &&
            ex_cp <= (sucs_ex_char_t)SUCS_SOFTCODE_RANGE_MAX);
}

static inline sucs_bancode_type_t extsucs_classify_bancode(sucs_ex_char_t ex_cp) {
    if (extsucs_is_bancode(ex_cp))   return SUCS_BANCODE_FATAL;
    if (extsucs_is_warncode(ex_cp))  return SUCS_BANCODE_WARN;
    if (extsucs_is_comcode(ex_cp))   return SUCS_BANCODE_COM;
    if (extsucs_is_softcode(ex_cp))  return SUCS_BANCODE_SOFT;
    return SUCS_BANCODE_NONE;
}

/* ============================================================================
 * Zero-Cost Upcasting: Base SUCS (31-bit) -> ExtSUCS (64-bit)
 *
 * Pure widening cast. No validation needed — all valid Base SUCS codepoints
 * are valid ExtSUCS codepoints, and the Base SUCS sentinel 0x7FFFFFFF
 * becomes a valid codepoint in the unbounded ExtSUCS space.
 * ============================================================================ */
static inline sucs_ex_char_t sucs_upcast(sucs_char_t cp) {
    return (sucs_ex_char_t)cp;
}

/* ============================================================================
 * Safe Downcasting: ExtSUCS (64-bit) -> Base SUCS (31-bit)
 *
 * Returns true on success, false if the codepoint exceeds the Base SUCS
 * 31-bit boundary (> 0x7FFFFFFF), equals the Base SUCS sentinel
 * (0x7FFFFFFF), or falls in the trap range.
 *
 * Note: 0x7FFFFFFF is valid in ExtSUCS but is the sentinel in Base SUCS,
 * so downcasting it fails — it cannot be represented in Base SUCS.
 *
 * Out-of-band error signaling: success/failure via return value,
 * decoded codepoint via output pointer.
 * ============================================================================ */
static inline bool sucs_downcast(sucs_ex_char_t ex_cp, sucs_char_t* out_cp) {
    if (!out_cp) {
        return false;
    }
    if (ex_cp > EXTSUCS_BASE_SUCS_MAX) {
        *out_cp = SUCS_INVALID_CODEPOINT;
        return false;
    }
    sucs_char_t cp = (sucs_char_t)(ex_cp & 0x7FFFFFFFUL);
    /* Reject Base SUCS sentinel — valid in ExtSUCS but not in Base SUCS */
    if (cp == SUCS_INVALID_CODEPOINT) {
        *out_cp = SUCS_INVALID_CODEPOINT;
        return false;
    }
    /* Reject Kernel Security Trap Range */
    if (cp >= SUCS_TRAP_RANGE_MIN && cp <= SUCS_TRAP_RANGE_MAX) {
        *out_cp = SUCS_INVALID_CODEPOINT;
        return false;
    }
    *out_cp = cp;
    return true;
}

#endif /* EXTSUCS_TYPES_H */
