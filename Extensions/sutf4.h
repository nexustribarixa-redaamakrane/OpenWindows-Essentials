/*
 * sutf4.h - SUTF-4 Transformation Format
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/sutf/include/sutf4.h semantics and byte-format exactly.
 *
 * SUTF-4 defines the endian-neutral mapping between SUCS codepoints and
 * 4-bit packed nibble sequences (8 nibbles / 4 bytes per codepoint) for
 * debugging, console dumps, and low-level bus transforms. Physical byte
 * packing and stream framing are defined by the SUST layer (see <sust.h>);
 * this header is strictly the transformation.
 *
 * SUTF-4 packs each SUCS codepoint as 8 nibbles (4 bits each) into a
 * fixed 4-byte frame. Nibbles advance most-significant-first; two nibbles
 * share each byte with the high nibble first:
 *
 *   byte0 = nibble0(hi) nibble1(lo)   cp bits 31..24
 *   byte1 = nibble2(hi) nibble3(lo)   cp bits 23..16
 *   byte2 = nibble4(hi) nibble5(lo)   cp bits 15..8
 *   byte3 = nibble6(hi) nibble7(lo)   cp bits  7..0
 *
 * Designed for console and bus debugging dumps.
 *
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only, zero allocation.
 */

#ifndef OWE_SUTF4_H
#define OWE_SUTF4_H

#include "sucs_types.h"

#define SUTF4_NIBBLES_PER_CODEPOINT 8
#define SUTF4_BYTES_PER_CODEPOINT   4

static inline size_t sutf4_codepoint_length(sucs_char_t cp)
{
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    return SUTF4_NIBBLES_PER_CODEPOINT;
}

/* Encodes a SUCS codepoint into the SUTF-4 4-bit nibble stream format.
 * Returns bytes written (always SUTF4_BYTES_PER_CODEPOINT), or 0 on error. */
static inline size_t sutf4_encode_char(
    sucs_char_t cp,
    uint8_t    *out_buf,
    size_t      buf_bytes)
{
    if (out_buf == NULL || !sucs_is_valid(cp)) {
        return 0;
    }

    if (buf_bytes < SUTF4_BYTES_PER_CODEPOINT) {
        return 0;
    }

    out_buf[0] = (uint8_t)(((cp >> 28) & 0x0FUL) << 4) | (uint8_t)((cp >> 24) & 0x0FUL);
    out_buf[1] = (uint8_t)(((cp >> 20) & 0x0FUL) << 4) | (uint8_t)((cp >> 16) & 0x0FUL);
    out_buf[2] = (uint8_t)(((cp >> 12) & 0x0FUL) << 4) | (uint8_t)((cp >> 8)  & 0x0FUL);
    out_buf[3] = (uint8_t)(((cp >> 4)  & 0x0FUL) << 4) | (uint8_t)(cp         & 0x0FUL);

    return SUTF4_BYTES_PER_CODEPOINT;
}

/* Decodes a SUTF-4 4-bit nibble stream into a SUCS codepoint.
 * Returns bytes read (always SUTF4_BYTES_PER_CODEPOINT), or 0 on error
 * (out_cp set to SUCS_INVALID_CODEPOINT). */
static inline size_t sutf4_decode_char(
    const uint8_t *in_buf,
    size_t         buf_bytes,
    sucs_char_t   *out_cp)
{
    if (out_cp == NULL) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (in_buf == NULL || buf_bytes < SUTF4_BYTES_PER_CODEPOINT) {
        return 0;
    }

    sucs_char_t cp = (((sucs_char_t)(in_buf[0] >> 4) & 0x0FUL) << 28) |
                     (((sucs_char_t)(in_buf[0] & 0x0FUL))      << 24) |
                     (((sucs_char_t)(in_buf[1] >> 4) & 0x0FUL) << 20) |
                     (((sucs_char_t)(in_buf[1] & 0x0FUL))      << 16) |
                     (((sucs_char_t)(in_buf[2] >> 4) & 0x0FUL) << 12) |
                     (((sucs_char_t)(in_buf[2] & 0x0FUL))      << 8)  |
                     (((sucs_char_t)(in_buf[3] >> 4) & 0x0FUL) << 4)  |
                     ((sucs_char_t)(in_buf[3] & 0x0FUL));

    if (!sucs_is_valid(cp)) {
        return 0;
    }

    *out_cp = cp;
    return SUTF4_BYTES_PER_CODEPOINT;
}

#endif /* OWE_SUTF4_H */