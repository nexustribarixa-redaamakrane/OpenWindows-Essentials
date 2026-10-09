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

/* ExtSUCS-typed block lookups. The block table is 1:1 with the base library
 * (ExtSUCS has no blocks of its own above the bridge), so these validate the
 * codepoint and delegate. */
static inline int extsucs_ucd_block_lookup(sucs_ex_char_t ex_cp) {
    if (ex_cp > (sucs_ex_char_t)SUCS_UNICODE_MAX_COMPAT) return -1;
    return sucs_ucd_block_lookup((sucs_char_t)ex_cp);
}

static inline bool extsucs_is_in_ucd_block(sucs_ex_char_t ex_cp, int block_index) {
    if (ex_cp > (sucs_ex_char_t)SUCS_UNICODE_MAX_COMPAT) return false;
    return sucs_is_in_ucd_block((sucs_char_t)ex_cp, block_index);
}

/* The UCS/UCB data set is shared with the base library, so ExtSUCS tracks the
 * same Unicode release. Aliased here so extended-only code does not have to
 * reach for the base SUCS_ names. */
#define EXTSUCS_UNICODE_VERSION_MAJOR SUCS_UNICODE_VERSION_MAJOR
#define EXTSUCS_UNICODE_VERSION_MINOR SUCS_UNICODE_VERSION_MINOR
#define EXTSUCS_UCD_UNICODE_VERSION  SUCS_UCD_UNICODE_VERSION
#define EXTSUCS_UCD_BLOCK_COUNT      SUCS_UCD_BLOCK_COUNT

#endif /* EXTSUCS_COMPAT_H */
