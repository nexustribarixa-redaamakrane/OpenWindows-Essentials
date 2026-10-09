#include "sutf2.h"

size_t sutf2_encode_char(sucs_char_t cp, uint8_t* out_buf, size_t buf_bytes) {
    if (!out_buf || !sucs_is_valid(cp)) {
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

size_t sutf2_decode_char(const uint8_t* in_buf, size_t buf_bytes, sucs_char_t* out_cp) {
    if (!out_cp) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (!in_buf || buf_bytes < SUTF2_BYTES_PER_CODEPOINT) {
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
