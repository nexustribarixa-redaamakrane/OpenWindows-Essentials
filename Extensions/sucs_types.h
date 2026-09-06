/*
 * sucs_types.h - SuperUnicode (SUCS) Character Encoding
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/sutf/sucs_types.h semantics so the identical typedef and
 * constants can safely coexist in a single translation unit.
 *
 * SUCS is strictly a CHARACTER ENCODING defining the abstract 31-bit
 * codepoint numerical address space (0x00000000 to 0x7FFFFFFF).
 *
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only.
 */

#ifndef OWE_SUCS_TYPES_H
#define OWE_SUCS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  SUCS Codepoint Address Space                                       */
/* ------------------------------------------------------------------ */

#ifndef SUCS_CHAR_T_DEFINED
#define SUCS_CHAR_T_DEFINED
typedef uint32_t sucs_char_t;
#endif

/* Sentinels and character encoding boundaries. */
#ifndef SUCS_INVALID_CODEPOINT
#define SUCS_INVALID_CODEPOINT 0x7FFFFFFFUL
#endif
#ifndef SUCS_MAX_CODEPOINT
#define SUCS_MAX_CODEPOINT     0x7FFFFFFFUL
#endif

/* SUCS kernel security trap range (aligned with BANcode trap range). */
#ifndef SUCS_TRAP_RANGE_MIN
#define SUCS_TRAP_RANGE_MIN    0x7FFFFFF0UL
#endif
#ifndef SUCS_TRAP_RANGE_MAX
#define SUCS_TRAP_RANGE_MAX    0x7FFFFFFEUL
#endif

/* ------------------------------------------------------------------ */
/* Base-mode transformation selection (SUTF-8/16/4/2; SUST-16 available
 * via <sust16.h>). Guarded so the identical enum can coexist with
 * canonical sucs_mode.h in a single translation unit. */
#ifndef SUCS_KERNEL_MODE_T_DEFINED
#define SUCS_KERNEL_MODE_T_DEFINED
typedef enum {
    SUCS_MODE_BASE     = 0,  /* Base SUCS (31-bit) & Base SUTF              */
    SUCS_MODE_EXTENDED = 1   /* ExtSUCS (unbounded 64-bit) & vSUTF + SUST  */
} sucs_kernel_mode_t;
#endif

/* ------------------------------------------------------------------ */
/*  Codepoint Validator                                                */
/* ------------------------------------------------------------------ */

#ifndef SUCS_SUCS_IS_VALID_DEFINED
#define SUCS_SUCS_IS_VALID_DEFINED
static inline bool sucs_is_valid(sucs_char_t cp)
{
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

#endif /* OWE_SUCS_TYPES_H */