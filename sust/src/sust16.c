#include "sust16.h"

size_t sust16_encode_bytes(sucs_char_t cp, uint8_t* out_bytes, size_t buf_bytes) {
    /* Canonical order == big-endian. */
    return sust16_encode_bytes_be(cp, out_bytes, buf_bytes);
}

size_t sust16_decode_bytes(const uint8_t* in_bytes, size_t buf_bytes, sucs_char_t* out_cp) {
    /* Canonical order == big-endian. */
    return sust16_decode_bytes_be(in_bytes, buf_bytes, out_cp);
}

size_t sust16_encode_bytes_be(sucs_char_t cp, uint8_t* out_bytes, size_t buf_bytes) {
    if (!out_bytes || !sucs_is_valid(cp)) {
        return 0;
    }

    if (cp <= 0x7FFFUL) {
        if (buf_bytes < 2) return 0;
        out_bytes[0] = (uint8_t)((cp >> 8) & 0x7FU);
        out_bytes[1] = (uint8_t)(cp & 0xFFU);
        return 2;
    }

    if (buf_bytes < 4) return 0;
    uint16_t w0 = (uint16_t)(0x8000U | ((cp >> 16) & 0x7FFFUL));
    uint16_t w1 = (uint16_t)(cp & 0xFFFFUL);
    out_bytes[0] = (uint8_t)((w0 >> 8) & 0xFFU);
    out_bytes[1] = (uint8_t)(w0 & 0xFFU);
    out_bytes[2] = (uint8_t)((w1 >> 8) & 0xFFU);
    out_bytes[3] = (uint8_t)(w1 & 0xFFU);
    return 4;
}

size_t sust16_decode_bytes_be(const uint8_t* in_bytes, size_t buf_bytes, sucs_char_t* out_cp) {
    if (!out_cp) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (!in_bytes || buf_bytes < 2) {
        return 0;
    }

    uint16_t w0 = (uint16_t)(((uint16_t)in_bytes[0] << 8) | (uint16_t)in_bytes[1]);

    if (w0 & 0x8000U) {
        /* Bit 15 set on the first word is the 2-word marker; a lone marker
         * word indicates a truncated 2-word serialization. */
        if (buf_bytes < 4) return 0;
        uint16_t w1 = (uint16_t)(((uint16_t)in_bytes[2] << 8) | (uint16_t)in_bytes[3]);
        sucs_char_t cp = (((sucs_char_t)(w0 & 0x7FFFU)) << 16) |
                         (sucs_char_t)w1;
        /* Overlong: values that fit in the 1-word form (<= 0x7FFF) must not
         * be carried in the 2-word form. */
        if (cp <= 0x7FFFUL) return 0;
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 4;
    }

    sucs_char_t cp = (sucs_char_t)w0;
    if (!sucs_is_valid(cp)) return 0;
    *out_cp = cp;
    return 2;
}

size_t sust16_encode_bytes_le(sucs_char_t cp, uint8_t* out_bytes, size_t buf_bytes) {
    if (!out_bytes || !sucs_is_valid(cp)) {
        return 0;
    }

    if (cp <= 0x7FFFUL) {
        if (buf_bytes < 2) return 0;
        out_bytes[0] = (uint8_t)(cp & 0xFFU);
        out_bytes[1] = (uint8_t)((cp >> 8) & 0x7FU);
        return 2;
    }

    if (buf_bytes < 4) return 0;
    uint16_t w0 = (uint16_t)(0x8000U | ((cp >> 16) & 0x7FFFUL));
    uint16_t w1 = (uint16_t)(cp & 0xFFFFUL);
    out_bytes[0] = (uint8_t)(w0 & 0xFFU);
    out_bytes[1] = (uint8_t)((w0 >> 8) & 0xFFU);
    out_bytes[2] = (uint8_t)(w1 & 0xFFU);
    out_bytes[3] = (uint8_t)((w1 >> 8) & 0xFFU);
    return 4;
}

size_t sust16_decode_bytes_le(const uint8_t* in_bytes, size_t buf_bytes, sucs_char_t* out_cp) {
    if (!out_cp) {
        return 0;
    }
    *out_cp = SUCS_INVALID_CODEPOINT;

    if (!in_bytes || buf_bytes < 2) {
        return 0;
    }

    uint16_t w0 = (uint16_t)((uint16_t)in_bytes[0] | ((uint16_t)in_bytes[1] << 8));

    if (w0 & 0x8000U) {
        /* Bit 15 set on the first word is the 2-word marker; a lone marker
         * word indicates a truncated 2-word serialization. */
        if (buf_bytes < 4) return 0;
        uint16_t w1 = (uint16_t)((uint16_t)in_bytes[2] | ((uint16_t)in_bytes[3] << 8));
        sucs_char_t cp = (((sucs_char_t)(w0 & 0x7FFFU)) << 16) |
                         (sucs_char_t)w1;
        /* Overlong: values that fit in the 1-word form (<= 0x7FFF) must not
         * be carried in the 2-word form. */
        if (cp <= 0x7FFFUL) return 0;
        if (!sucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 4;
    }

    sucs_char_t cp = (sucs_char_t)w0;
    if (!sucs_is_valid(cp)) return 0;
    *out_cp = cp;
    return 2;
}