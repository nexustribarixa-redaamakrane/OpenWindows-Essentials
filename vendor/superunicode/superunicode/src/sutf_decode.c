#include "superunicode/sutf.h"

int sutf_decode_char(const char* in_buf, size_t buf_size, sucs_char_t* out_cp, size_t* out_bytes_read) {
    if (!in_buf || !out_cp || !out_bytes_read) {
        return SUES_ERR_INVALID_BYTE;
    }

    if (buf_size == 0) {
        return SUES_ERR_BUFFER_TOO_SMALL;
    }

    unsigned char u0 = (unsigned char)in_buf[0];

    if ((u0 & 0x80U) == 0x00U) {
        /* 1-byte ASCII / SUTF sequence */
        *out_cp = (sucs_char_t)u0;
        *out_bytes_read = 1;
        return SUES_SUCCESS;
    } else if ((u0 & 0xE0U) == 0xC0U) {
        /* 2-byte sequence */
        if (buf_size < 2) return SUES_ERR_BUFFER_TOO_SMALL;
        unsigned char u1 = (unsigned char)in_buf[1];
        if ((u1 & 0xC0U) != 0x80U) return SUES_ERR_INVALID_BYTE;

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x1FU)) << 6) |
                         ((sucs_char_t)(u1 & 0x3FU));
        if (cp < 0x80UL) return SUES_ERR_INVALID_BYTE; /* overlong */
        *out_cp = cp;
        *out_bytes_read = 2;
        return SUES_SUCCESS;
    } else if ((u0 & 0xF0U) == 0xE0U) {
        /* 3-byte sequence */
        if (buf_size < 3) return SUES_ERR_BUFFER_TOO_SMALL;
        unsigned char u1 = (unsigned char)in_buf[1];
        unsigned char u2 = (unsigned char)in_buf[2];
        if ((u1 & 0xC0U) != 0x80U || (u2 & 0xC0U) != 0x80U) return SUES_ERR_INVALID_BYTE;

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x0FU)) << 12) |
                         (((sucs_char_t)(u1 & 0x3FU)) << 6) |
                         ((sucs_char_t)(u2 & 0x3FU));
        if (cp < 0x800UL) return SUES_ERR_INVALID_BYTE; /* overlong */
        *out_cp = cp;
        *out_bytes_read = 3;
        return SUES_SUCCESS;
    } else if ((u0 & 0xF8U) == 0xF0U) {
        /* 4-byte sequence */
        if (buf_size < 4) return SUES_ERR_BUFFER_TOO_SMALL;
        unsigned char u1 = (unsigned char)in_buf[1];
        unsigned char u2 = (unsigned char)in_buf[2];
        unsigned char u3 = (unsigned char)in_buf[3];
        if ((u1 & 0xC0U) != 0x80U || (u2 & 0xC0U) != 0x80U || (u3 & 0xC0U) != 0x80U) {
            return SUES_ERR_INVALID_BYTE;
        }

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x07U)) << 18) |
                         (((sucs_char_t)(u1 & 0x3FU)) << 12) |
                         (((sucs_char_t)(u2 & 0x3FU)) << 6) |
                         ((sucs_char_t)(u3 & 0x3FU));
        /* 4-byte framing covers only the Unicode-compatible range 0x10000-0x10FFFF */
        if (cp < 0x10000UL) return SUES_ERR_INVALID_BYTE;   /* overlong */
        if (cp > 0x0010FFFFUL) return SUES_ERR_INVALID_BYTE; /* must use 5-byte */
        *out_cp = cp;
        *out_bytes_read = 4;
        return SUES_SUCCESS;
    } else if ((u0 & 0xFCU) == 0xF8U) {
        /* 5-byte sequence */
        if (buf_size < 5) return SUES_ERR_BUFFER_TOO_SMALL;
        unsigned char u1 = (unsigned char)in_buf[1];
        unsigned char u2 = (unsigned char)in_buf[2];
        unsigned char u3 = (unsigned char)in_buf[3];
        unsigned char u4 = (unsigned char)in_buf[4];
        if ((u1 & 0xC0U) != 0x80U || (u2 & 0xC0U) != 0x80U ||
            (u3 & 0xC0U) != 0x80U || (u4 & 0xC0U) != 0x80U) {
            return SUES_ERR_INVALID_BYTE;
        }

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x03U)) << 24) |
                         (((sucs_char_t)(u1 & 0x3FU)) << 18) |
                         (((sucs_char_t)(u2 & 0x3FU)) << 12) |
                         (((sucs_char_t)(u3 & 0x3FU)) << 6) |
                         ((sucs_char_t)(u4 & 0x3FU));
        /* 5-byte framing covers native extended space 0x110000-0x3FFFFFF */
        if (cp < 0x00110000UL) return SUES_ERR_INVALID_BYTE; /* overlong (should be 4-byte) */
        if (cp > 0x03FFFFFFUL) return SUES_ERR_INVALID_BYTE; /* must use 6-byte */
        *out_cp = cp;
        *out_bytes_read = 5;
        return SUES_SUCCESS;
    } else if ((u0 & 0xFEU) == 0xFCU) {
        /* 6-byte sequence */
        if (buf_size < 6) return SUES_ERR_BUFFER_TOO_SMALL;
        unsigned char u1 = (unsigned char)in_buf[1];
        unsigned char u2 = (unsigned char)in_buf[2];
        unsigned char u3 = (unsigned char)in_buf[3];
        unsigned char u4 = (unsigned char)in_buf[4];
        unsigned char u5 = (unsigned char)in_buf[5];
        if ((u1 & 0xC0U) != 0x80U || (u2 & 0xC0U) != 0x80U ||
            (u3 & 0xC0U) != 0x80U || (u4 & 0xC0U) != 0x80U ||
            (u5 & 0xC0U) != 0x80U) {
            return SUES_ERR_INVALID_BYTE;
        }

        sucs_char_t cp = (((sucs_char_t)(u0 & 0x01U)) << 30) |
                         (((sucs_char_t)(u1 & 0x3FU)) << 24) |
                         (((sucs_char_t)(u2 & 0x3FU)) << 18) |
                         (((sucs_char_t)(u3 & 0x3FU)) << 12) |
                         (((sucs_char_t)(u4 & 0x3FU)) << 6) |
                         ((sucs_char_t)(u5 & 0x3FU));

        if (cp > SUCS_MAX_CODEPOINT) {
            return SUES_ERR_OUT_OF_BOUNDS;
        }
        /* Overlong 6-byte encoding (value fits in 5 bytes) */
        if (cp < 0x04000000UL) {
            return SUES_ERR_INVALID_BYTE;
        }
        /* Kernel Security Trap range and sentinel are reserved, not encodable */
        if (cp >= SUCS_KERNEL_TRAP_MIN && cp <= SUCS_KERNEL_TRAP_MAX) {
            return SUES_ERR_INVALID_CODEPOINT;
        }
        if (cp == SUCS_INVALID_CODEPOINT) {
            return SUES_ERR_INVALID_CODEPOINT;
        }

        *out_cp = cp;
        *out_bytes_read = 6;
        return SUES_SUCCESS;
    }

    return SUES_ERR_INVALID_BYTE;
}
