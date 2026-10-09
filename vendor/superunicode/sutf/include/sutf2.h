#ifndef SUTF_SUTF2_H
#define SUTF_SUTF2_H

#include "sucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SUTF-2 Transformation Format
 *
 * SUTF-2 defines the endian-neutral mapping between SUCS codepoints and
 * 2-bit symbol frames (16 frames / 4 bytes per codepoint) for compressed
 * bitstream IPC thread channel transforms.
 */
#define SUTF2_FRAMES_PER_CODEPOINT 16
#define SUTF2_BYTES_PER_CODEPOINT  4

static inline size_t sutf2_codepoint_length(sucs_char_t cp) {
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    return SUTF2_FRAMES_PER_CODEPOINT;
}

/**
 * Encodes a SUCS codepoint into the SUTF-2 2-bit symbol frame transport format.
 * Returns bytes written, or 0 on error.
 */
size_t sutf2_encode_char(sucs_char_t cp, uint8_t* out_buf, size_t buf_bytes);

/**
 * Decodes a SUTF-2 2-bit symbol frame transport format into a SUCS character encoding codepoint.
 * Returns bytes read, or 0 on error (yielding SUCS_INVALID_CODEPOINT).
 */
size_t sutf2_decode_char(const uint8_t* in_buf, size_t buf_bytes, sucs_char_t* out_cp);

#ifdef __cplusplus
}
#endif

#endif /* SUTF_SUTF2_H */
