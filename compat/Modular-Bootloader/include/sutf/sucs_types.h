#ifndef SUTF_SUCS_TYPES_H
#define SUTF_SUCS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * SuperUnicode (SUCS) Character Encoding Specification
 * 
 * SUCS (SuperUnicode) is strictly a CHARACTER ENCODING defining the abstract
 * 31-bit codepoint numerical address space (0x00000000 to 0x7FFFFFFF).
 *
 * For the unbounded (0 -> infinity) ExtSUCS Character Encoding and the
 * extended transformation format vSUTF, see <extsucs_types.h> and
 * <vsutf.h> in superunicode_extended.
 *
 * SUTF is strictly a transformation format (endian-neutral codepoint <->
 * symbol-sequence mapping). Physical byte serialization, bit-packing, and
 * stream framing are defined by the SUST (SuperUnicode Serialization
 * Transports) library — see <sust.h> (SUST-16/32/64/128/256/512/N, e-SUST).
 */
/* Guarded so the identical typedef can safely coexist with superunicode/
 * sucs_types.h and extsucs_types.h in a single translation unit. */
#ifndef SUCS_CHAR_T_DEFINED
#define SUCS_CHAR_T_DEFINED
typedef uint32_t sucs_char_t;
#endif

/* Sentinels and Character Encoding Boundaries.
 * Guarded so identical constants can coexist with superunicode/sucs_types.h
 * and extsucs_types.h in a single translation unit. */
#ifndef SUCS_INVALID_CODEPOINT
#define SUCS_INVALID_CODEPOINT 0x7FFFFFFFUL
#endif
#ifndef SUCS_MAX_CODEPOINT
#define SUCS_MAX_CODEPOINT     0x7FFFFFFFUL
#endif

/* Kernel Security Trap Range in SUCS Address Space.
 * Guarded so identical constants can coexist in a single translation unit. */
#ifndef SUCS_TRAP_RANGE_MIN
#define SUCS_TRAP_RANGE_MIN    0x7FFFFFF0UL
#endif
#ifndef SUCS_TRAP_RANGE_MAX
#define SUCS_TRAP_RANGE_MAX    0x7FFFFFFEUL
#endif

/**
 * Codepoint Validator
 * Checks whether a given SUCS codepoint address is valid within the character encoding map.
 */
#ifndef SUCS_SUCS_IS_VALID_DEFINED
#define SUCS_SUCS_IS_VALID_DEFINED
static inline bool sucs_is_valid(sucs_char_t cp) {
    if (cp > SUCS_MAX_CODEPOINT) {
        return false;
    }
    if (cp >= SUCS_TRAP_RANGE_MIN && cp <= SUCS_TRAP_RANGE_MAX) {
        return false;
    }
    if (cp == SUCS_INVALID_CODEPOINT) {
        return false;
    }
    return true;
}
#endif

/* ============================================================================
 * ExtSUCS Character Encoding & Transformation Format Forwarding References
 * ============================================================================ */
#if defined(__has_include)
  #if __has_include("extsucs_types.h")
    #include "extsucs_types.h"
  #endif
#endif

#endif /* SUTF_SUCS_TYPES_H */
