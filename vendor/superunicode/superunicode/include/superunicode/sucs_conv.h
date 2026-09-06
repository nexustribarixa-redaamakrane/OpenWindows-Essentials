/**
 * @file sucs_conv.h
 * @brief Bidirectional Encoding Conversion between SuperUnicode (SUCS) and Unicode
 *
 * Provides stream-level converters:
 *   - UTF-8 <-> SUTF-8 (utf2sutf / sutf2utf)
 *   - Unicode codepoints <-> SUCS codepoints (unicode2superunicode / superunicode2unicode)
 *
 * SUTF-8 is a strict superset of UTF-8 for the Unicode-compatible range
 * (U+0000 to U+10FFFF).  For codepoints in that range, SUTF-8 byte
 * sequences are identical to UTF-8.  The divergence begins at 5-byte
 * (0x00110000–0x03FFFFFF) and 6-byte (0x04000000–0x7FFFFFFF) sequences
 * which encode the native SUCS extended address space that has no
 * Unicode equivalent.
 *
 * Unicode codepoints occupy the SUCS "Zone 0 — Unicode Parity Zone"
 * (0x00000000–0x0010FFFF), which is a direct 1:1 identity mapping.
 * Codepoints beyond 0x10FFFF are native SUCS and have no Unicode mapping.
 */

#ifndef SUCS_CONV_H
#define SUCS_CONV_H

#include "sucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================= */
/* Encoding Conversion Status Codes                                          */
/* ========================================================================= */

typedef enum {
    SUCS_CONV_OK                     =  0,
    SUCS_CONV_ERR_NULL_POINTER       = -1,
    SUCS_CONV_ERR_BUFFER_TOO_SMALL   = -2,
    SUCS_CONV_ERR_INVALID_INPUT      = -3,
    SUCS_CONV_ERR_OUT_OF_RANGE       = -4,  /* Codepoint outside target encoding's range */
    SUCS_CONV_ERR_TRAP_RANGE         = -5   /* Kernel Security Trap range (not convertible) */
} sucs_conv_status_t;

/* ========================================================================= */
/* UTF-8 <-> SUTF-8 Stream Converters                                        */
/* ========================================================================= */

/**
 * @brief Converts a UTF-8 encoded byte stream into SUTF-8 encoding.
 *
 * For the Unicode-compatible range (U+0000 to U+10FFFF), UTF-8 and SUTF-8
 * produce identical byte sequences, so this is effectively a passthrough
 * with validation.  Invalid UTF-8 sequences are rejected.
 *
 * @param utf8_in          Input UTF-8 byte buffer.
 * @param utf8_len         Length of input buffer in bytes.
 * @param sutf8_out        Output SUTF-8 byte buffer.
 * @param sutf8_capacity   Capacity of output buffer in bytes.
 * @param out_bytes_written Number of bytes written to sutf8_out.
 * @return                 SUCS_CONV_OK on success, or negative error code.
 */
sucs_conv_status_t sucs_conv_utf8_to_sutf8(const uint8_t *utf8_in, size_t utf8_len,
                                           uint8_t *sutf8_out, size_t sutf8_capacity,
                                           size_t *out_bytes_written);

/**
 * @brief Converts a SUTF-8 encoded byte stream into UTF-8 encoding.
 *
 * Only codepoints in the Unicode-compatible range (U+0000 to U+10FFFF) can
 * be converted.  Native SUCS codepoints beyond U+10FFFF are replaced with
 * U+FFFD (REPLACEMENT CHARACTER) or cause SUCS_CONV_ERR_OUT_OF_RANGE if
 * strict mode is enabled.
 *
 * @param sutf8_in         Input SUTF-8 byte buffer.
 * @param sutf8_len        Length of input buffer in bytes.
 * @param utf8_out         Output UTF-8 byte buffer.
 * @param utf8_capacity    Capacity of output buffer in bytes.
 * @param out_bytes_written Number of bytes written to utf8_out.
 * @param strict           If true, out-of-range codepoints cause an error.
 *                         If false, they are replaced with U+FFFD.
 * @return                 SUCS_CONV_OK on success, or negative error code.
 */
sucs_conv_status_t sucs_conv_sutf8_to_utf8(const uint8_t *sutf8_in, size_t sutf8_len,
                                           uint8_t *utf8_out, size_t utf8_capacity,
                                           size_t *out_bytes_written, bool strict);

/* ========================================================================= */
/* Unicode <-> SuperUnicode Codepoint Array Converters                       */
/* ========================================================================= */

/**
 * @brief Converts an array of Unicode codepoints (uint32_t, max U+10FFFF)
 *        into SUCS codepoints (sucs_char_t).
 *
 * This is a 1:1 identity mapping for U+0000 to U+10FFFF.  Values beyond
 * U+10FFFF are rejected.
 *
 * @param unicode_in       Input array of Unicode codepoints.
 * @param count            Number of codepoints in the array.
 * @param sucs_out         Output array of SUCS codepoints.
 * @param out_count        Number of codepoints written.
 * @return                 SUCS_CONV_OK on success, or negative error code.
 */
sucs_conv_status_t sucs_conv_unicode_to_sucs(const uint32_t *unicode_in, size_t count,
                                             sucs_char_t *sucs_out, size_t *out_count);

/**
 * @brief Converts an array of SUCS codepoints (sucs_char_t) into Unicode
 *        codepoints (uint32_t, max U+10FFFF).
 *
 * Only SUCS codepoints in the Unicode Parity Zone (U+0000 to U+10FFFF)
 * can be converted.  Codepoints beyond that range are replaced with U+FFFD
 * in lenient mode or cause an error in strict mode.
 *
 * @param sucs_in          Input array of SUCS codepoints.
 * @param count            Number of codepoints in the array.
 * @param unicode_out      Output array of Unicode codepoints.
 * @param out_count        Number of codepoints written.
 * @param strict           If true, out-of-range codepoints cause an error.
 *                         If false, they are replaced with U+FFFD.
 * @return                 SUCS_CONV_OK on success, or negative error code.
 */
sucs_conv_status_t sucs_conv_sucs_to_unicode(const sucs_char_t *sucs_in, size_t count,
                                             uint32_t *unicode_out, size_t *out_count,
                                             bool strict);

#ifdef __cplusplus
}
#endif

#endif /* SUCS_CONV_H */
