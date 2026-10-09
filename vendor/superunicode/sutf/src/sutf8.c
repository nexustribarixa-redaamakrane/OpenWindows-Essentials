#include "sutf8.h"

size_t sutf8_encode_char(sucs_char_t cp, uint8_t* out_buf, size_t buf_size) {
    if (!out_buf || !sucs_is_valid(cp)) {
        return 0;
    }

    if (cp <= 0x7FUL) {
        if (buf_size < 1) return 0;
        out_buf[0] = (uint8_t)(cp & 0x7FUL);
        return 1;
    } else if (cp <= 0x7FFUL) {
        if (buf_size < 2) return 0;
        out_buf[0] = (uint8_t)(0xC0UL | ((cp >> 6) & 0x1FUL));
        out_buf[1] = (uint8_t)(0x80UL | (cp & 0x3FUL));
        return 2;
    } else if (cp <= 0xFFFFUL) {
        if (buf_size < 3) return 0;
        out_buf[0] = (uint8_t)(0xE0UL | ((cp >> 12) & 0x0FUL));
        out_buf[1] = (uint8_t)(0x80UL | ((cp >> 6) & 0x3FUL));
        out_buf[2] = (uint8_t)(0x80UL | (cp & 0x3FUL));
        return 3;
    } else if (cp <= 0x0010FFFFUL) {
        /* 4-byte framing covers only the Unicode-compatible range (0x10000-0x10FFFF) */
        if (buf_size < 4) return 0;
        out_buf[0] = (uint8_t)(0xF0UL | ((cp >> 18) & 0x07UL));
        out_buf[1] = (uint8_t)(0x80UL | ((cp >> 12) & 0x3FUL));
        out_buf[2] = (uint8_t)(0x80UL | ((cp >> 6) & 0x3FUL));
        out_buf[3] = (uint8_t)(0x80UL | (cp & 0x3FUL));
        return 4;
    } else if (cp <= 0x03FFFFFFUL) {
        /* 5-byte framing covers native extended space (0x110000-0x3FFFFFF) */
        if (buf_size < 5) return 0;
        out_buf[0] = (uint8_t)(0xF8UL | ((cp >> 24) & 0x03UL));
        out_buf[1] = (uint8_t)(0x80UL | ((cp >> 18) & 0x3FUL));
        out_buf[2] = (uint8_t)(0x80UL | ((cp >> 12) & 0x3FUL));
        out_buf[3] = (uint8_t)(0x80UL | ((cp >> 6) & 0x3FUL));
        out_buf[4] = (uint8_t)(0x80UL | (cp & 0x3FUL));
        return 5;
    } else if (cp <= 0x7FFFFFFFUL) {
        if (buf_size < 6) return 0;
        out_buf[0] = (uint8_t)(0xFCUL | ((cp >> 30) & 0x01UL));
        out_buf[1] = (uint8_t)(0x80UL | ((cp >> 24) & 0x3FUL));
        out_buf[2] = (uint8_t)(0x80UL | ((cp >> 18) & 0x3FUL));
        out_buf[3] = (uint8_t)(0x80UL | ((cp >> 12) & 0x3FUL));
        out_buf[4] = (uint8_t)(0x80UL | ((cp >> 6) & 0x3FUL));
        out_buf[5] = (uint8_t)(0x80UL | (cp & 0x3FUL));
        return 6;
    }

    return 0;
}

size_t sutf8_decode_char(const uint8_t* in_buf, size_t buf_size, sucs_char_t* out_cp) {
    if (!out_cp) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (!in_buf || buf_size == 0) {
        return 0;
    }

    uint8_t u0 = in_buf[0];

    if ((u0 & 0x80U) == 0x00U) {
        sucs_char_t cp = (sucs_char_t)u0;
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 1;
    } else if ((u0 & 0xE0U) == 0xC0U) {
        if (buf_size < 2) return 0;
        uint8_t u1 = in_buf[1];
        if ((u1 & 0xC0U) != 0x80U) return 0;

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x1FU)) << 6) |
                         ((sucs_char_t)(u1 & 0x3FU));
        if (cp < 0x80UL) return 0; /* overlong */
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 2;
    } else if ((u0 & 0xF0U) == 0xE0U) {
        if (buf_size < 3) return 0;
        uint8_t u1 = in_buf[1];
        uint8_t u2 = in_buf[2];
        if ((u1 & 0xC0U) != 0x80U || (u2 & 0xC0U) != 0x80U) return 0;

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x0FU)) << 12) |
                         (((sucs_char_t)(u1 & 0x3FU)) << 6) |
                         ((sucs_char_t)(u2 & 0x3FU));
        if (cp < 0x800UL) return 0; /* overlong */
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 3;
    } else if ((u0 & 0xF8U) == 0xF0U) {
        if (buf_size < 4) return 0;
        uint8_t u1 = in_buf[1];
        uint8_t u2 = in_buf[2];
        uint8_t u3 = in_buf[3];
        if ((u1 & 0xC0U) != 0x80U || (u2 & 0xC0U) != 0x80U || (u3 & 0xC0U) != 0x80U) return 0;

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x07U)) << 18) |
                         (((sucs_char_t)(u1 & 0x3FU)) << 12) |
                         (((sucs_char_t)(u2 & 0x3FU)) << 6) |
                         ((sucs_char_t)(u3 & 0x3FU));
        if (cp < 0x10000UL) return 0;      /* overlong (must be 3-byte) */
        if (cp > 0x0010FFFFUL) return 0;   /* must use 5-byte framing */
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 4;
    } else if ((u0 & 0xFCU) == 0xF8U) {
        if (buf_size < 5) return 0;
        uint8_t u1 = in_buf[1];
        uint8_t u2 = in_buf[2];
        uint8_t u3 = in_buf[3];
        uint8_t u4 = in_buf[4];
        if ((u1 & 0xC0U) != 0x80U || (u2 & 0xC0U) != 0x80U ||
            (u3 & 0xC0U) != 0x80U || (u4 & 0xC0U) != 0x80U) return 0;

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x03U)) << 24) |
                         (((sucs_char_t)(u1 & 0x3FU)) << 18) |
                         (((sucs_char_t)(u2 & 0x3FU)) << 12) |
                         (((sucs_char_t)(u3 & 0x3FU)) << 6) |
                         ((sucs_char_t)(u4 & 0x3FU));
        if (cp < 0x00110000UL) return 0;   /* overlong (must be 4-byte) */
        if (cp > 0x03FFFFFFUL) return 0;   /* must use 6-byte framing */
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 5;
    } else if ((u0 & 0xFEU) == 0xFCU) {
        if (buf_size < 6) return 0;
        uint8_t u1 = in_buf[1];
        uint8_t u2 = in_buf[2];
        uint8_t u3 = in_buf[3];
        uint8_t u4 = in_buf[4];
        uint8_t u5 = in_buf[5];
        if ((u1 & 0xC0U) != 0x80U || (u2 & 0xC0U) != 0x80U ||
            (u3 & 0xC0U) != 0x80U || (u4 & 0xC0U) != 0x80U ||
            (u5 & 0xC0U) != 0x80U) return 0;

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x01U)) << 30) |
                         (((sucs_char_t)(u1 & 0x3FU)) << 24) |
                         (((sucs_char_t)(u2 & 0x3FU)) << 18) |
                         (((sucs_char_t)(u3 & 0x3FU)) << 12) |
                         (((sucs_char_t)(u4 & 0x3FU)) << 6) |
                         ((sucs_char_t)(u5 & 0x3FU));
        if (cp < 0x04000000UL) return 0; /* overlong (must be 5-byte) */
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 6;
    }

    return 0;
}
