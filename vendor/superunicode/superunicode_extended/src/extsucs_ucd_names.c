#include "extsucs_ucd_names.h"

/*
 * Thin wrappers over superunicode's UCD name database.
 * All Unicode codepoints fit in 32 bits, so we validate the ExtSUCS
 * codepoint and delegate to the base library.
 */
#include "superunicode/sucs_ucd_names.h"

const char* extsucs_ucd_get_name(sucs_ex_char_t cp) {
    if (cp > (sucs_ex_char_t)SUCS_UNICODE_MAX_COMPAT) return (const char*)0;
    return sucs_ucd_get_name((sucs_char_t)cp);
}

uint16_t extsucs_ucd_name_length(sucs_ex_char_t cp) {
    if (cp > (sucs_ex_char_t)SUCS_UNICODE_MAX_COMPAT) return 0;
    return sucs_ucd_name_length((sucs_char_t)cp);
}

uint16_t extsucs_ucd_get_name_copy(sucs_ex_char_t cp, char* out_buf, uint16_t buf_size) {
    if (cp > (sucs_ex_char_t)SUCS_UNICODE_MAX_COMPAT) return 0;
    return sucs_ucd_get_name_copy((sucs_char_t)cp, out_buf, buf_size);
}
