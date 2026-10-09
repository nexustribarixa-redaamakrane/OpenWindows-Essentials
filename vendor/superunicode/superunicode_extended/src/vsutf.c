/**
 * vSUTF (Variable SUTF) Text Formatting and Serialization Transport
 *
 * Implementation of the vSUTF multi-byte variable-length stream transport
 * for ExtSUCS character encoding codepoints across the full unbounded
 * (0 -> infinity) address space.
 *
 * Base SUCS fast-path (0x00-0x7FFFFFFF): Standard SUTF-8 framing (1-6 bytes).
 * Extended range (> 0x7FFFFFFF): 0xFE prefix + 8-byte big-endian (9 bytes).
 *
 * Zero standard library dependencies.
 */

#include "vsutf.h"

/* ============================================================================
 * vSUTF Encode: ExtSUCS codepoint -> variable-length byte stream
 *
 * Base SUCS range uses SUTF-8 compatible framing.
 * Extended range uses 0xFE prefix + 8-byte big-endian payload.
 * ============================================================================ */
size_t vsutf_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size) {
    if (!out_buf || !extsucs_is_valid(ex_cp)) {
        return 0;
    }

    /* Base SUCS fast-path: SUTF-8 compatible framing (1-6 bytes) */
    if (ex_cp <= 0x7FULL) {
        if (buf_size < 1) return 0;
        out_buf[0] = (uint8_t)(ex_cp & 0x7FULL);
        return 1;
    } else if (ex_cp <= 0x7FFULL) {
        if (buf_size < 2) return 0;
        out_buf[0] = (uint8_t)(0xC0ULL | ((ex_cp >> 6) & 0x1FULL));
        out_buf[1] = (uint8_t)(0x80ULL | (ex_cp & 0x3FULL));
        return 2;
    } else if (ex_cp <= 0xFFFFULL) {
        if (buf_size < 3) return 0;
        out_buf[0] = (uint8_t)(0xE0ULL | ((ex_cp >> 12) & 0x0FULL));
        out_buf[1] = (uint8_t)(0x80ULL | ((ex_cp >> 6) & 0x3FULL));
        out_buf[2] = (uint8_t)(0x80ULL | (ex_cp & 0x3FULL));
        return 3;
    } else if (ex_cp <= 0x0010FFFFULL) {
        /* 4-byte framing covers only the Unicode-compatible range (0x10000-0x10FFFF) */
        if (buf_size < 4) return 0;
        out_buf[0] = (uint8_t)(0xF0ULL | ((ex_cp >> 18) & 0x07ULL));
        out_buf[1] = (uint8_t)(0x80ULL | ((ex_cp >> 12) & 0x3FULL));
        out_buf[2] = (uint8_t)(0x80ULL | ((ex_cp >> 6) & 0x3FULL));
        out_buf[3] = (uint8_t)(0x80ULL | (ex_cp & 0x3FULL));
        return 4;
    } else if (ex_cp <= 0x03FFFFFFULL) {
        /* 5-byte framing covers native extended space (0x110000-0x3FFFFFF) */
        if (buf_size < 5) return 0;
        out_buf[0] = (uint8_t)(0xF8ULL | ((ex_cp >> 24) & 0x03ULL));
        out_buf[1] = (uint8_t)(0x80ULL | ((ex_cp >> 18) & 0x3FULL));
        out_buf[2] = (uint8_t)(0x80ULL | ((ex_cp >> 12) & 0x3FULL));
        out_buf[3] = (uint8_t)(0x80ULL | ((ex_cp >> 6) & 0x3FULL));
        out_buf[4] = (uint8_t)(0x80ULL | (ex_cp & 0x3FULL));
        return 5;
    } else if (ex_cp <= 0x7FFFFFFFULL) {
        if (buf_size < 6) return 0;
        out_buf[0] = (uint8_t)(0xFCULL | ((ex_cp >> 30) & 0x01ULL));
        out_buf[1] = (uint8_t)(0x80ULL | ((ex_cp >> 24) & 0x3FULL));
        out_buf[2] = (uint8_t)(0x80ULL | ((ex_cp >> 18) & 0x3FULL));
        out_buf[3] = (uint8_t)(0x80ULL | ((ex_cp >> 12) & 0x3FULL));
        out_buf[4] = (uint8_t)(0x80ULL | ((ex_cp >> 6) & 0x3FULL));
        out_buf[5] = (uint8_t)(0x80ULL | (ex_cp & 0x3FULL));
        return 6;
    }

    /* Extended range (> 0x7FFFFFFF): 0xFE prefix + 8-byte big-endian */
    if (buf_size < VSUTF_EXT64_TOTAL) return 0;
    out_buf[0] = VSUTF_EXT64_PREFIX;
    out_buf[1] = (uint8_t)((ex_cp >> 56) & 0xFFULL);
    out_buf[2] = (uint8_t)((ex_cp >> 48) & 0xFFULL);
    out_buf[3] = (uint8_t)((ex_cp >> 40) & 0xFFULL);
    out_buf[4] = (uint8_t)((ex_cp >> 32) & 0xFFULL);
    out_buf[5] = (uint8_t)((ex_cp >> 24) & 0xFFULL);
    out_buf[6] = (uint8_t)((ex_cp >> 16) & 0xFFULL);
    out_buf[7] = (uint8_t)((ex_cp >> 8)  & 0xFFULL);
    out_buf[8] = (uint8_t)(ex_cp         & 0xFFULL);
    return VSUTF_EXT64_TOTAL;
}

/* ============================================================================
 * vSUTF Decode: variable-length byte stream -> ExtSUCS codepoint
 *
 * Detects lead byte pattern to determine framing:
 * - 0x00-0x7F: 1-byte ASCII
 * - 0xC0-0xDF: 2-byte SUTF-8
 * - 0xE0-0xEF: 3-byte SUTF-8
 * - 0xF0-0xF7: 4-byte SUTF-8
 * - 0xF8-0xFB: 5-byte SUTF-8
 * - 0xFC-0xFD: 6-byte SUTF-8
 * - 0xFE:      9-byte extended (64-bit)
 * - 0xFF:      Reserved (returns error)
 * ============================================================================ */
size_t vsutf_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp) {
    if (!in_buf || !out_cp || buf_size == 0) {
        return 0;
    }

    uint8_t u0 = in_buf[0];

    /* 0xFF: Reserved prefix — reject */
    if (u0 == VSUTF_RESERVED_PREFIX) {
        return 0;
    }

    /* 0xFE: Extended 64-bit transport (1 prefix + 8 payload = 9 bytes) */
    if (u0 == VSUTF_EXT64_PREFIX) {
        if (buf_size < VSUTF_EXT64_TOTAL) return 0;
        sucs_ex_char_t cp = 0;
        cp |= ((sucs_ex_char_t)in_buf[1]) << 56;
        cp |= ((sucs_ex_char_t)in_buf[2]) << 48;
        cp |= ((sucs_ex_char_t)in_buf[3]) << 40;
        cp |= ((sucs_ex_char_t)in_buf[4]) << 32;
        cp |= ((sucs_ex_char_t)in_buf[5]) << 24;
        cp |= ((sucs_ex_char_t)in_buf[6]) << 16;
        cp |= ((sucs_ex_char_t)in_buf[7]) << 8;
        cp |= ((sucs_ex_char_t)in_buf[8]);
        if (cp <= 0x7FFFFFFFULL) return 0; /* overlong: must use base framing */
        if (!extsucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return VSUTF_EXT64_TOTAL;
    }

    /* Base SUCS fast-path: SUTF-8 compatible decoding (1-6 bytes) */
    if ((u0 & 0x80U) == 0x00U) {
        sucs_ex_char_t cp = (sucs_ex_char_t)u0;
        if (!extsucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 1;
    } else if ((u0 & 0xE0U) == 0xC0U) {
        if (buf_size < 2) return 0;
        if ((in_buf[1] & 0xC0U) != 0x80U) return 0;
        sucs_ex_char_t cp = (((sucs_ex_char_t)(u0 & 0x1FU)) << 6) |
                            ((sucs_ex_char_t)(in_buf[1] & 0x3FU));
        if (cp < 0x80ULL) return 0; /* overlong */
        if (!extsucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 2;
    } else if ((u0 & 0xF0U) == 0xE0U) {
        if (buf_size < 3) return 0;
        if ((in_buf[1] & 0xC0U) != 0x80U || (in_buf[2] & 0xC0U) != 0x80U) return 0;
        sucs_ex_char_t cp = (((sucs_ex_char_t)(u0 & 0x0FU)) << 12) |
                            (((sucs_ex_char_t)(in_buf[1] & 0x3FU)) << 6) |
                            ((sucs_ex_char_t)(in_buf[2] & 0x3FU));
        if (cp < 0x800ULL) return 0; /* overlong */
        if (!extsucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 3;
    } else if ((u0 & 0xF8U) == 0xF0U) {
        if (buf_size < 4) return 0;
        if ((in_buf[1] & 0xC0U) != 0x80U || (in_buf[2] & 0xC0U) != 0x80U ||
            (in_buf[3] & 0xC0U) != 0x80U) return 0;
        sucs_ex_char_t cp = (((sucs_ex_char_t)(u0 & 0x07U)) << 18) |
                            (((sucs_ex_char_t)(in_buf[1] & 0x3FU)) << 12) |
                            (((sucs_ex_char_t)(in_buf[2] & 0x3FU)) << 6) |
                            ((sucs_ex_char_t)(in_buf[3] & 0x3FU));
        if (cp < 0x10000ULL) return 0;     /* overlong (must be 3-byte) */
        if (cp > 0x0010FFFFULL) return 0;  /* must use 5-byte framing */
        if (!extsucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 4;
    } else if ((u0 & 0xFCU) == 0xF8U) {
        if (buf_size < 5) return 0;
        if ((in_buf[1] & 0xC0U) != 0x80U || (in_buf[2] & 0xC0U) != 0x80U ||
            (in_buf[3] & 0xC0U) != 0x80U || (in_buf[4] & 0xC0U) != 0x80U) return 0;
        sucs_ex_char_t cp = (((sucs_ex_char_t)(u0 & 0x03U)) << 24) |
                            (((sucs_ex_char_t)(in_buf[1] & 0x3FU)) << 18) |
                            (((sucs_ex_char_t)(in_buf[2] & 0x3FU)) << 12) |
                            (((sucs_ex_char_t)(in_buf[3] & 0x3FU)) << 6) |
                            ((sucs_ex_char_t)(in_buf[4] & 0x3FU));
        if (cp < 0x00110000ULL) return 0;  /* overlong (must be 4-byte) */
        if (cp > 0x03FFFFFFULL) return 0;  /* must use 6-byte framing */
        if (!extsucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 5;
    } else if ((u0 & 0xFEU) == 0xFCU) {
        if (buf_size < 6) return 0;
        if ((in_buf[1] & 0xC0U) != 0x80U || (in_buf[2] & 0xC0U) != 0x80U ||
            (in_buf[3] & 0xC0U) != 0x80U || (in_buf[4] & 0xC0U) != 0x80U ||
            (in_buf[5] & 0xC0U) != 0x80U) return 0;
        sucs_ex_char_t cp = (((sucs_ex_char_t)(u0 & 0x01U)) << 30) |
                            (((sucs_ex_char_t)(in_buf[1] & 0x3FU)) << 24) |
                            (((sucs_ex_char_t)(in_buf[2] & 0x3FU)) << 18) |
                            (((sucs_ex_char_t)(in_buf[3] & 0x3FU)) << 12) |
                            (((sucs_ex_char_t)(in_buf[4] & 0x3FU)) << 6) |
                            ((sucs_ex_char_t)(in_buf[5] & 0x3FU));
        if (cp < 0x04000000ULL) return 0; /* overlong (must be 5-byte) */
        if (!extsucs_is_valid(cp)) return 0;
        *out_cp = cp;
        return 6;
    }

    /* Continuation byte or invalid lead byte */
    return 0;
}
