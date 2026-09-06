#ifndef SUPERUNICODE_SUCS_PLANE_H
#define SUPERUNICODE_SUCS_PLANE_H

#include "sucs_types.h"

/* Coordinate Extraction Macros
 *
 * 31-bit address space partition (128 Zones x 256 Districts x 256 Planes x
 * 256 Block Offsets = 2^31):
 * - Zone    : Bits 24..30 (7 bits) -> 128 zones
 * - District: Bits 16..23 (8 bits) -> 256 districts per zone
 * - Plane   : Bits  8..15 (8 bits) -> 256 planes per district
 * - Offset  : Bits  0.. 7 (8 bits) -> 256 block offsets per plane
 */
#define SUCS_GET_ZONE(cp)     (((cp) >> 24) & 0x7FUL)
#define SUCS_GET_DISTRICT(cp) (((cp) >> 16) & 0xFFUL)
#define SUCS_GET_PLANE(cp)    (((cp) >> 8)  & 0xFFUL)
#define SUCS_GET_OFFSET(cp)   ((cp) & 0xFFUL)

/* Inline Coordinate & Property Helpers */

static inline bool sucs_is_fixed_plane(sucs_char_t cp) {
    uint32_t plane = SUCS_GET_PLANE(cp);
    return (plane == 0UL || plane == 1UL);
}

static inline sucs_codepoint_type_t sucs_classify_codepoint(sucs_char_t cp) {
    if (!sucs_is_valid(cp)) {
        /* Rejects cp > 0x7FFFFFFF, the Kernel Security Trap range, and the
         * in-band sentinel — none of these are encodable Base SUCS codepoints. */
        return SUCS_TYPE_INVALID;
    } else if (cp <= 0x0010FFFFUL) {
        return SUCS_TYPE_UNICODE_COMPAT;
    } else if (cp >= SUCS_SCP_MIN && cp <= SUCS_SCP_MAX) {
        return SUCS_TYPE_SYS_FUNCTION;
    } else {
        return SUCS_TYPE_NATIVE_ALLOC;
    }
}

static inline bool sucs_is_unicode_compatible(sucs_char_t cp) {
    return (cp <= 0x0010FFFFUL);
}

static inline bool sucs_is_scp_plane(sucs_char_t cp) {
    return (cp >= SUCS_SCP_MIN && cp <= SUCS_SCP_MAX);
}

static inline bool sucs_is_formatting_char(sucs_char_t cp) {
    return sucs_is_scp_plane(cp);
}

/* BANcode Registry & Kernel Trap Damage Control Dispatch */

static inline bool sucs_is_bancode_registry(sucs_char_t cp) {
    return (cp >= SUCS_BANCODE_REGISTRY_MIN && cp <= SUCS_BANCODE_REGISTRY_MAX);
}

static inline sucs_bancode_type_t sucs_classify_bancode(sucs_char_t cp) {
    if (cp >= SUCS_BANCODE_RANGE_MIN && cp <= SUCS_BANCODE_RANGE_MAX) {
        return SUCS_BANCODE_FATAL;
    } else if (cp >= SUCS_WARNCODE_RANGE_MIN && cp <= SUCS_WARNCODE_RANGE_MAX) {
        return SUCS_BANCODE_WARN;
    } else if (cp >= SUCS_COMCODE_RANGE_MIN && cp <= SUCS_COMCODE_RANGE_MAX) {
        return SUCS_BANCODE_COM;
    } else if (cp >= SUCS_SOFTCODE_RANGE_MIN && cp <= SUCS_SOFTCODE_RANGE_MAX) {
        return SUCS_BANCODE_SOFT;
    } else {
        return SUCS_BANCODE_NONE;
    }
}

static inline bool sucs_is_bancode(sucs_char_t cp) {
    return (cp >= SUCS_BANCODE_RANGE_MIN && cp <= SUCS_BANCODE_RANGE_MAX);
}

static inline bool sucs_is_warncode(sucs_char_t cp) {
    return (cp >= SUCS_WARNCODE_RANGE_MIN && cp <= SUCS_WARNCODE_RANGE_MAX);
}

static inline bool sucs_is_comcode(sucs_char_t cp) {
    return (cp >= SUCS_COMCODE_RANGE_MIN && cp <= SUCS_COMCODE_RANGE_MAX);
}

static inline bool sucs_is_softcode(sucs_char_t cp) {
    return (cp >= SUCS_SOFTCODE_RANGE_MIN && cp <= SUCS_SOFTCODE_RANGE_MAX);
}

static inline bool sucs_is_kernel_trap(sucs_char_t cp) {
    return (cp >= SUCS_KERNEL_TRAP_MIN && cp <= SUCS_KERNEL_TRAP_MAX);
}

/**
 * Resolves the Kernel Security Trap codepoint (0x7FFFFFF0-0x7FFFFFFE) governing
 * a B+ BANcode for damage control dispatch upon kernel crash. Returns
 * SUCS_INVALID_CODEPOINT if the input is not a B+ BANcode or falls beyond the
 * 15 assigned trap slots (0x0011A780-0x0011A7FF is unmapped).
 */
static inline sucs_char_t sucs_bancode_to_trap(sucs_char_t bancode_cp) {
    if (!sucs_is_bancode(bancode_cp)) {
        return SUCS_INVALID_CODEPOINT;
    }
    uint32_t slot = (bancode_cp - SUCS_BANCODE_RANGE_MIN) / SUCS_BANCODES_PER_TRAP;
    if (slot >= SUCS_TRAP_SLOT_COUNT) {
        return SUCS_INVALID_CODEPOINT;
    }
    return (sucs_char_t)(SUCS_KERNEL_TRAP_MIN + slot);
}

/**
 * Returns the B+ BANcode range (inclusive min/max) managed by a specific
 * Kernel Security Trap handler. Returns false for non-trap codepoints or null
 * output pointers.
 */
static inline bool sucs_trap_to_bancode_range(sucs_char_t trap_cp, sucs_char_t* out_min, sucs_char_t* out_max) {
    if (!sucs_is_kernel_trap(trap_cp) || out_min == NULL || out_max == NULL) {
        return false;
    }
    uint32_t slot = (uint32_t)(trap_cp - SUCS_KERNEL_TRAP_MIN);
    *out_min = (sucs_char_t)(SUCS_BANCODE_RANGE_MIN + slot * SUCS_BANCODES_PER_TRAP);
    *out_max = (sucs_char_t)(*out_min + (SUCS_BANCODES_PER_TRAP - 1UL));
    return true;
}

#endif /* SUPERUNICODE_SUCS_PLANE_H */
