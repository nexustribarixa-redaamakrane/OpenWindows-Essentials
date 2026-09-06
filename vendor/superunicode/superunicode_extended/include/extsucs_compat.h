#ifndef EXTSUCS_COMPAT_H
#define EXTSUCS_COMPAT_H

/*
 * ExtSUCS Unicode Compatibility Layer
 *
 * Inherits ALL block range defines, lookup tables, and sucs_char_t helpers
 * from superunicode/sucs_compat.h.  Only adds ExtSUCS-typed (sucs_ex_char_t)
 * wrappers for the compat and native-extended predicates.
 */

#include "extsucs_types.h"
#include "superunicode/sucs_compat.h"

/* ExtSUCS-typed Unicode compatibility helpers */
static inline bool extsucs_is_unicode_compat(sucs_ex_char_t ex_cp) {
    return (ex_cp <= (sucs_ex_char_t)SUCS_UNICODE_MAX_COMPAT);
}

static inline bool extsucs_is_native_extended(sucs_ex_char_t ex_cp) {
    return (ex_cp > (sucs_ex_char_t)SUCS_UNICODE_MAX_COMPAT &&
            ex_cp <= (sucs_ex_char_t)SUCS_MAX_CODEPOINT);
}

#endif /* EXTSUCS_COMPAT_H */
