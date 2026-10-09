/**
 * @file sucs_conv.c
 * @brief Bidirectional Encoding Conversion between SuperUnicode (SUCS) and Unicode
 */

#include "superunicode/sucs_conv.h"
#include "superunicode/sutf.h"
#include "superunicode/sucs_plane.h"

/* Helper to encode a single Unicode codepoint (0..0x10FFFF) into UTF-8 */
static int utf8_encode_char(uint32_t cp, uint8_t *out, size_t cap, size_t *written) {
    if (!out || !written) return -1;

    if (cp > 0x10FFFFUL || (cp >= 0xD800UL && cp <= 0xDFFFUL)) {
        return -1;
    }

    if (cp <= 0x7FUL) {
        if (cap < 1) return -2;
        out[0] = (uint8_t)cp;
        *written = 1;
    } else if (cp <= 0x7FFUL) {
        if (cap < 2) return -2;
        out[0] = (uint8_t)(0xC0UL | (cp >> 6));
        out[1] = (uint8_t)(0x80UL | (cp & 0x3FUL));
        *written = 2;
    } else if (cp <= 0xFFFFUL) {
        if (cap < 3) return -2;
        out[0] = (uint8_t)(0xE0UL | (cp >> 12));
        out[1] = (uint8_t)(0x80UL | ((cp >> 6) & 0x3FUL));
        out[2] = (uint8_t)(0x80UL | (cp & 0x3FUL));
        *written = 3;
    } else {
        if (cap < 4) return -2;
        out[0] = (uint8_t)(0xF0UL | (cp >> 18));
        out[1] = (uint8_t)(0x80UL | ((cp >> 12) & 0x3FUL));
        out[2] = (uint8_t)(0x80UL | ((cp >> 6) & 0x3FUL));
        out[3] = (uint8_t)(0x80UL | (cp & 0x3FUL));
        *written = 4;
    }
    return 0;
}

/* Helper to decode a single UTF-8 sequence */
static int utf8_decode_char(const uint8_t *in, size_t len, uint32_t *out_cp, size_t *consumed) {
    if (!in || !out_cp || !consumed) return -1;
    if (len == 0) return -2;

    uint8_t b0 = in[0];
    if ((b0 & 0x80U) == 0x00U) {
        *out_cp = b0;
        *consumed = 1;
        return 0;
    } else if ((b0 & 0xE0U) == 0xC0U) {
        if (len < 2) return -2;
        uint8_t b1 = in[1];
        if ((b1 & 0xC0U) != 0x80U) return -3;
        uint32_t cp = (((uint32_t)(b0 & 0x1FU)) << 6) | ((uint32_t)(b1 & 0x3FU));
        if (cp < 0x80UL) return -3; /* overlong */
        *out_cp = cp;
        *consumed = 2;
        return 0;
    } else if ((b0 & 0xF0U) == 0xE0U) {
        if (len < 3) return -2;
        uint8_t b1 = in[1];
        uint8_t b2 = in[2];
        if ((b1 & 0xC0U) != 0x80U || (b2 & 0xC0U) != 0x80U) return -3;
        uint32_t cp = (((uint32_t)(b0 & 0x0FU)) << 12) |
                      (((uint32_t)(b1 & 0x3FU)) << 6) |
                      ((uint32_t)(b2 & 0x3FU));
        if (cp < 0x800UL) return -3; /* overlong */
        if (cp >= 0xD800UL && cp <= 0xDFFFUL) return -3; /* surrogates */
        *out_cp = cp;
        *consumed = 3;
        return 0;
    } else if ((b0 & 0xF8U) == 0xF0U) {
        if (len < 4) return -2;
        uint8_t b1 = in[1];
        uint8_t b2 = in[2];
        uint8_t b3 = in[3];
        if ((b1 & 0xC0U) != 0x80U || (b2 & 0xC0U) != 0x80U || (b3 & 0xC0U) != 0x80U) return -3;
        uint32_t cp = (((uint32_t)(b0 & 0x07U)) << 18) |
                      (((uint32_t)(b1 & 0x3FU)) << 12) |
                      (((uint32_t)(b2 & 0x3FU)) << 6) |
                      ((uint32_t)(b3 & 0x3FU));
        if (cp < 0x10000UL || cp > 0x10FFFFUL) return -3; /* overlong or out of Unicode */
        *out_cp = cp;
        *consumed = 4;
        return 0;
    }
    return -3;
}

sucs_conv_status_t sucs_conv_utf8_to_sutf8(const uint8_t *utf8_in, size_t utf8_len,
                                           uint8_t *sutf8_out, size_t sutf8_capacity,
                                           size_t *out_bytes_written) {
    if (!utf8_in || !sutf8_out || !out_bytes_written) {
        return SUCS_CONV_ERR_NULL_POINTER;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < utf8_len) {
        uint32_t cp = 0;
        size_t read_bytes = 0;
        int d_res = utf8_decode_char(utf8_in + in_pos, utf8_len - in_pos, &cp, &read_bytes);
        if (d_res != 0) {
            return SUCS_CONV_ERR_INVALID_INPUT;
        }
        in_pos += read_bytes;

        char sutf_buf[8];
        size_t written = 0;
        int e_res = sutf_encode_char((sucs_char_t)cp, sutf_buf, sizeof(sutf_buf), &written);
        if (e_res != SUES_SUCCESS) {
            return SUCS_CONV_ERR_INVALID_INPUT;
        }

        if (out_pos + written > sutf8_capacity) {
            return SUCS_CONV_ERR_BUFFER_TOO_SMALL;
        }

        for (size_t i = 0; i < written; i++) {
            sutf8_out[out_pos + i] = (uint8_t)sutf_buf[i];
        }
        out_pos += written;
    }

    *out_bytes_written = out_pos;
    return SUCS_CONV_OK;
}

sucs_conv_status_t sucs_conv_sutf8_to_utf8(const uint8_t *sutf8_in, size_t sutf8_len,
                                           uint8_t *utf8_out, size_t utf8_capacity,
                                           size_t *out_bytes_written, bool strict) {
    if (!sutf8_in || !utf8_out || !out_bytes_written) {
        return SUCS_CONV_ERR_NULL_POINTER;
    }

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos < sutf8_len) {
        sucs_char_t cp = 0;
        size_t read_bytes = 0;
        int d_res = sutf_decode_char((const char*)(sutf8_in + in_pos), sutf8_len - in_pos, &cp, &read_bytes);
        if (d_res != SUES_SUCCESS) {
            return SUCS_CONV_ERR_INVALID_INPUT;
        }
        in_pos += read_bytes;

        /* Check if codepoint fits in standard Unicode (0..0x10FFFF) and is not a surrogate */
        if (cp <= 0x10FFFFUL && (cp < 0xD800UL || cp > 0xDFFFUL)) {
            size_t written = 0;
            int e_res = utf8_encode_char(cp, utf8_out + out_pos, utf8_capacity - out_pos, &written);
            if (e_res == -2) {
                return SUCS_CONV_ERR_BUFFER_TOO_SMALL;
            }
            if (e_res != 0) {
                return SUCS_CONV_ERR_INVALID_INPUT;
            }
            out_pos += written;
        } else {
            /* Out of Unicode range or surrogate */
            if (strict) {
                return SUCS_CONV_ERR_OUT_OF_RANGE;
            }
            /* Emit U+FFFD (REPLACEMENT CHARACTER: 0xEF 0xBF 0xBD) */
            if (out_pos + 3 > utf8_capacity) {
                return SUCS_CONV_ERR_BUFFER_TOO_SMALL;
            }
            utf8_out[out_pos++] = 0xEF;
            utf8_out[out_pos++] = 0xBF;
            utf8_out[out_pos++] = 0xBD;
        }
    }

    *out_bytes_written = out_pos;
    return SUCS_CONV_OK;
}

sucs_conv_status_t sucs_conv_unicode_to_sucs(const uint32_t *unicode_in, size_t count,
                                             sucs_char_t *sucs_out, size_t *out_count) {
    if (!unicode_in || !sucs_out || !out_count) {
        return SUCS_CONV_ERR_NULL_POINTER;
    }

    for (size_t i = 0; i < count; i++) {
        uint32_t cp = unicode_in[i];
        if (cp > 0x10FFFFUL || (cp >= 0xD800UL && cp <= 0xDFFFUL)) {
            return SUCS_CONV_ERR_OUT_OF_RANGE;
        }
        sucs_out[i] = (sucs_char_t)cp;
    }

    *out_count = count;
    return SUCS_CONV_OK;
}

sucs_conv_status_t sucs_conv_sucs_to_unicode(const sucs_char_t *sucs_in, size_t count,
                                             uint32_t *unicode_out, size_t *out_count,
                                             bool strict) {
    if (!sucs_in || !unicode_out || !out_count) {
        return SUCS_CONV_ERR_NULL_POINTER;
    }

    for (size_t i = 0; i < count; i++) {
        sucs_char_t cp = sucs_in[i];
        if (sucs_is_valid(cp) && cp <= 0x10FFFFUL && (cp < 0xD800UL || cp > 0xDFFFUL)) {
            unicode_out[i] = (uint32_t)cp;
        } else {
            if (strict) {
                return SUCS_CONV_ERR_OUT_OF_RANGE;
            }
            unicode_out[i] = 0x0000FFFDUL;
        }
    }

    *out_count = count;
    return SUCS_CONV_OK;
}
