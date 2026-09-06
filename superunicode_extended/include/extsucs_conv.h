/**
 * @file extsucs_conv.h
 * @brief Bidirectional Encoding Conversion between ExtSUCS / vSUTF and Unicode / UTF-8
 *
 * Provides stream and array level converters for ExtSUCS (SuperUnicode Extended):
 *   - UTF-8 <-> vSUTF (utf2sutf / sutf2utf)
 *   - Unicode codepoints (uint32_t) <-> ExtSUCS codepoints (sucs_ex_char_t 64-bit)
 */

#ifndef EXTSUCS_CONV_H
#define EXTSUCS_CONV_H

#include "extsucs_types.h"
#include "vsutf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EXTSUCS_CONV_OK                   =  0,
    EXTSUCS_CONV_ERR_NULL_POINTER     = -1,
    EXTSUCS_CONV_ERR_BUFFER_TOO_SMALL = -2,
    EXTSUCS_CONV_ERR_INVALID_INPUT    = -3,
    EXTSUCS_CONV_ERR_OUT_OF_RANGE     = -4,
    EXTSUCS_CONV_ERR_TRAP_RANGE       = -5
} extsucs_conv_status_t;

/**
 * @brief Converts a UTF-8 encoded byte stream into vSUTF (Variable SUTF) encoding.
 *
 * @param utf8_in           Input UTF-8 buffer.
 * @param utf8_len          Length of input in bytes.
 * @param vsutf_out         Output vSUTF buffer.
 * @param vsutf_capacity    Capacity of output buffer in bytes.
 * @param out_bytes_written Number of bytes written to vsutf_out.
 * @return                  EXTSUCS_CONV_OK on success, or error code.
 */
extsucs_conv_status_t extsucs_conv_utf8_to_vsutf(const uint8_t *utf8_in, size_t utf8_len,
                                                 uint8_t *vsutf_out, size_t vsutf_capacity,
                                                 size_t *out_bytes_written);

/**
 * @brief Converts a vSUTF byte stream into standard UTF-8 encoding.
 *
 * @param vsutf_in          Input vSUTF buffer.
 * @param vsutf_len         Length of input in bytes.
 * @param utf8_out          Output UTF-8 buffer.
 * @param utf8_capacity     Capacity of output buffer in bytes.
 * @param out_bytes_written Number of bytes written to utf8_out.
 * @param strict            If true, codepoints outside Unicode range (>0x10FFFF) fail.
 *                          If false, out-of-range codepoints are replaced with U+FFFD.
 * @return                  EXTSUCS_CONV_OK on success, or error code.
 */
extsucs_conv_status_t extsucs_conv_vsutf_to_utf8(const uint8_t *vsutf_in, size_t vsutf_len,
                                                 uint8_t *utf8_out, size_t utf8_capacity,
                                                 size_t *out_bytes_written, bool strict);

/**
 * @brief Converts an array of Unicode codepoints (uint32_t) to ExtSUCS (sucs_ex_char_t).
 */
extsucs_conv_status_t extsucs_conv_unicode_to_extsucs(const uint32_t *unicode_in, size_t count,
                                                      sucs_ex_char_t *extsucs_out, size_t *out_count);

/**
 * @brief Converts an array of ExtSUCS codepoints (sucs_ex_char_t) to Unicode (uint32_t).
 */
extsucs_conv_status_t extsucs_conv_extsucs_to_unicode(const sucs_ex_char_t *extsucs_in, size_t count,
                                                      uint32_t *unicode_out, size_t *out_count,
                                                      bool strict);

#ifdef __cplusplus
}
#endif

#endif /* EXTSUCS_CONV_H */
