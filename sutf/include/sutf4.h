#ifndef SUTF_SUTF4_H
#define SUTF_SUTF4_H

#include "sucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SUTF-4 Transformation Format
 *
 * SUTF-4 defines the endian-neutral mapping between SUCS codepoints and
 * 4-bit packed nibble sequences (8 nibbles / 4 bytes per codepoint) for
 * debugging, console dumps, and low-level bus transforms.
 */
#define SUTF4_NIBBLES_PER_CODEPOINT 8
#define SUTF4_BYTES_PER_CODEPOINT   4

static inline size_t sutf4_codepoint_length(sucs_char_t cp) {
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    return SUTF4_NIBBLES_PER_CODEPOINT;
}

/**
 * Encodes a SUCS codepoint into the SUTF-4 4-bit nibble stream format.
 * Returns bytes written, or 0 on error.
 */
size_t sutf4_encode_char(sucs_char_t cp, uint8_t* out_buf, size_t buf_bytes);

/**
 * Decodes a SUTF-4 4-bit nibble stream format into a SUCS character encoding codepoint.
 * Returns bytes read, or 0 on error (yielding SUCS_INVALID_CODEPOINT).
 */
size_t sutf4_decode_char(const uint8_t* in_buf, size_t buf_bytes, sucs_char_t* out_cp);

#ifdef __cplusplus
}
#endif

#endif /* SUTF_SUTF4_H */
