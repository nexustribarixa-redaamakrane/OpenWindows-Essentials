/*
 * sutf2.h - SUTF-2 Transformation Format
 *
 * Self-contained freestanding compatibility layer. Mirrors the canonical
 * superunicode/sutf/include/sutf2.h semantics and byte-format exactly.
 *
 * SUTF-2 defines the endian-neutral mapping between SUCS codepoints and
 * 2-bit symbol frames (16 frames / 4 bytes per codepoint) for compressed
 * bitstream IPC thread channel transforms. Physical byte packing and
 * stream framing are defined by the SUST layer (see <sust.h>); this
 * header is strictly the transformation.
 *
 * SUTF-2 frames each SUCS codepoint as 16 2-bit symbol frames (IPC thread
 * channel transport). On the wire the 32 bits are serialized MSB-first in
 * a fixed 4-byte frame:
 *
 *   byte0 = cp bits 31..24
 *   byte1 = cp bits 23..16
 *   byte2 = cp bits 15..8
 *   byte3 = cp bits  7..0
 *
 * C99 freestanding - stdint.h / stdbool.h / stddef.h only, zero allocation.
 */

#ifndef OWE_SUTF2_H
#define OWE_SUTF2_H

#include "sucs_types.h"

#define SUTF2_FRAMES_PER_CODEPOINT 16
#define SUTF2_BYTES_PER_CODEPOINT  4

static inline size_t sutf2_codepoint_length(sucs_char_t cp)
{
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    return SUTF2_FRAMES_PER_CODEPOINT;
}

/* Encodes a SUCS codepoint into the SUTF-2 transformation format.
 * Returns bytes written (always SUTF2_BYTES_PER_CODEPOINT), or 0 on error. */
static inline size_t sutf2_encode_char(
    sucs_char_t cp,
    uint8_t    *out_buf,
    size_t      buf_bytes)
{
    if (out_buf == NULL || !sucs_is_valid(cp)) {
        return 0;
    }

    if (buf_bytes < SUTF2_BYTES_PER_CODEPOINT) {
        return 0;
    }

    out_buf[0] = (uint8_t)((cp >> 24) & 0xFFUL);
    out_buf[1] = (uint8_t)((cp >> 16) & 0xFFUL);
    out_buf[2] = (uint8_t)((cp >> 8)  & 0xFFUL);
    out_buf[3] = (uint8_t)(cp         & 0xFFUL);

    return SUTF2_BYTES_PER_CODEPOINT;
}

/* Decodes a SUTF-2 transformation into a SUCS codepoint.
 * Returns bytes read (always SUTF2_BYTES_PER_CODEPOINT), or 0 on error
 * (out_cp set to SUCS_INVALID_CODEPOINT). */
static inline size_t sutf2_decode_char(
    const uint8_t *in_buf,
    size_t         buf_bytes,
    sucs_char_t   *out_cp)
{
    if (out_cp == NULL) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (in_buf == NULL || buf_bytes < SUTF2_BYTES_PER_CODEPOINT) {
        return 0;
    }

    sucs_char_t cp = (((sucs_char_t)in_buf[0]) << 24) |
                     (((sucs_char_t)in_buf[1]) << 16) |
                     (((sucs_char_t)in_buf[2]) << 8)  |
                     ((sucs_char_t)in_buf[3]);

    if (!sucs_is_valid(cp)) {
        return 0;
    }

    *out_cp = cp;
    return SUTF2_BYTES_PER_CODEPOINT;
}

#endif /* OWE_SUTF2_H */