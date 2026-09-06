#ifndef EXTSUCS_UCD_NAMES_H
#define EXTSUCS_UCD_NAMES_H

#include "extsucs_types.h"
#include "extsucs_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Unicode Character Database Name Lookup for ExtSUCS
 *
 * Inherits the UCD name database from superunicode.  Functions accept
 * sucs_ex_char_t and validate the codepoint is in the Unicode range
 * (0x00000000 - 0x0010FFFF) before delegating to the base library.
 * ======================================================================== */

/**
 * Returns the Unicode character name for a given ExtSUCS codepoint.
 * Returns NULL if cp is outside the Unicode range or has no UCD name.
 * Uses O(log n) binary search on the sorted index table.
 */
const char* extsucs_ucd_get_name(sucs_ex_char_t cp);

/**
 * Returns the length of the Unicode character name for a given ExtSUCS codepoint.
 * Returns 0 if cp is outside the Unicode range or has no UCD name.
 */
uint16_t extsucs_ucd_name_length(sucs_ex_char_t cp);

/**
 * Copies the Unicode character name for a given ExtSUCS codepoint into out_buf.
 * Returns the number of bytes copied, or 0 if not found or buffer too small.
 */
uint16_t extsucs_ucd_get_name_copy(sucs_ex_char_t cp, char* out_buf, uint16_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* EXTSUCS_UCD_NAMES_H */
