#ifndef SUPERUNICODE_SUPERUNICODE_H
#define SUPERUNICODE_SUPERUNICODE_H

#include "sucs_types.h"  /* IWYU pragma: export */
#include "sucs_plane.h"  /* IWYU pragma: export */
#include "sucs_compat.h" /* IWYU pragma: export */
#include "sucs_trap.h"   /* IWYU pragma: export */
#include "sutf.h"        /* IWYU pragma: export */
#include "sucs_conv.h"   /* IWYU pragma: export */

#ifdef __cplusplus
extern "C" {
#endif

/* Freestanding SUCS_STRING Operations */

/**
 * Counts printable code points in a SUCS_STRING descriptor.
 * Note: SUCS_TYPE_SYS_FUNCTION (System Control Plane) code points do NOT count toward printable width.
 */
int sucs_strlen(const SUCS_STRING* str, size_t* out_char_count);

/**
 * Counts total code points in a SUCS_STRING descriptor (including SCP formatting codes).
 */
int sucs_codepoint_count(const SUCS_STRING* str, size_t* out_cp_count);

/**
 * Safe bounded copy from src SUCS_STRING to dest SUCS_STRING.
 */
int sucs_strcpy(SUCS_STRING* dest, const SUCS_STRING* src);

/**
 * Compares two SUCS_STRING descriptors for byte equality.
 */
int sucs_streq(const SUCS_STRING* a, const SUCS_STRING* b, bool* out_equal);

#ifdef __cplusplus
}
#endif

#endif /* SUPERUNICODE_SUPERUNICODE_H */
