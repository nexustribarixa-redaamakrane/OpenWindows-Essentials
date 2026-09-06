#ifndef SUPERUNICODE_SUCS_UCD_NAMES_H
#define SUPERUNICODE_SUCS_UCD_NAMES_H

#include "sucs_types.h"
#include "sucs_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Unicode Character Database Name Lookup (Unicode 17.0)
 *
 * Provides codepoint -> character name resolution for all named
 * codepoints in the Unicode Compatibility Range (0x000000 - 0x10FFFF).
 * Data is stored in a packed string pool with a sorted index table
 * for O(log n) binary search lookup.
 * ======================================================================== */

/* Total named codepoints in database */
#define SUCS_UCD_NAME_COUNT 40470

/* Index entry: codepoint -> name pool offset */
typedef struct {
    uint32_t cp;           /* Unicode codepoint */
    uint32_t name_offset;  /* Byte offset into sucs_ucd_name_pool[] */
    uint16_t name_length;  /* Length of name string in bytes */
} sucs_ucd_name_entry_t;

/* Sorted index table (binary-searchable by codepoint) */
extern const sucs_ucd_name_entry_t sucs_ucd_name_index[SUCS_UCD_NAME_COUNT];

/* Packed string pool containing all character names */
extern const char sucs_ucd_name_pool[];

/* ========================================================================
 * Lookup API
 * ======================================================================== */

/**
 * Returns the Unicode character name for a given codepoint.
 * Returns NULL if the codepoint has no name in the UCD.
 * Uses O(log n) binary search on the sorted index table.
 */
const char* sucs_ucd_get_name(sucs_char_t cp);

/**
 * Returns the length of the Unicode character name for a given codepoint.
 * Returns 0 if the codepoint has no name in the UCD.
 */
uint16_t sucs_ucd_name_length(sucs_char_t cp);

/**
 * Copies the Unicode character name for a given codepoint into out_buf.
 * Returns the number of bytes copied, or 0 if not found or buffer too small.
 */
uint16_t sucs_ucd_get_name_copy(sucs_char_t cp, char* out_buf, uint16_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* SUPERUNICODE_SUCS_UCD_NAMES_H */
