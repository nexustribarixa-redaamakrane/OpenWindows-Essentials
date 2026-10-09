#include "sutf4.h"

size_t sutf4_encode_char(sucs_char_t cp, uint8_t* out_buf, size_t buf_bytes) {
    if (!out_buf || !sucs_is_valid(cp)) {
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

size_t sutf4_decode_char(const uint8_t* in_buf, size_t buf_bytes, sucs_char_t* out_cp) {
    if (!out_cp) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (!in_buf || buf_bytes < SUTF4_BYTES_PER_CODEPOINT) {
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
