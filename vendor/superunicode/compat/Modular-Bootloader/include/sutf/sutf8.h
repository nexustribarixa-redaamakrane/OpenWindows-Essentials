#ifndef SUTF_SUTF8_H
#define SUTF_SUTF8_H

#include "sucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SUTF-8 Transformation Format
 *
 * SUTF-8 defines the endian-neutral mapping between SUCS codepoints and
 * 1 to 6 byte sequences. Physical byte packing and stream framing rules
 * for serialized streams are defined by the SUST layer (see <sust.h>);
 * this header is strictly the transformation.
 *
 * Note: in this codebase byte-level validation of SUTF-8 is provided via
 * sutf8_encode_char/sutf8_decode_char — the byte array IS the symbol
 * sequence (SUTF-8 has no word-order ambiguity to normalize).
 */

/* Inline helper for SUTF-8 transport stream length calculation */
static inline size_t sutf8_codepoint_length(sucs_char_t cp) {
    if (!sucs_is_valid(cp)) {
        return 0;
    }
    if (cp <= 0x7FUL) {
        return 1;
    } else if (cp <= 0x7FFUL) {
        return 2;
    } else if (cp <= 0xFFFFUL) {
        return 3;
    } else if (cp <= 0x0010FFFFUL) {
        return 4;
    } else if (cp <= 0x3FFFFFFUL) {
        return 5;
    } else {
        return 6;
    }
}

/**
 * Encodes a SUCS codepoint into the SUTF-8 text formatting and transport stream.
 * Returns bytes written, or 0 on error.
 */
size_t sutf8_encode_char(sucs_char_t cp, uint8_t* out_buf, size_t buf_size);

/**
 * Decodes a SUTF-8 transport stream into a SUCS character encoding codepoint.
 * Returns bytes read, or 0 on error (yielding SUCS_INVALID_CODEPOINT).
 */
size_t sutf8_decode_char(const uint8_t* in_buf, size_t buf_size, sucs_char_t* out_cp);

#ifdef __cplusplus
}
#endif

#endif /* SUTF_SUTF8_H */
