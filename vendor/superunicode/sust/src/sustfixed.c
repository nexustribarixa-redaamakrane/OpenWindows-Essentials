/**
 * SUST Fixed-Width Serialization Transports
 *
 * Implementation of SUST-32, SUST-64, SUST-128, SUST-256, SUST-512,
 * and SUST-N fixed-width vector block serialization transports for
 * SUCS and ExtSUCS codepoints. All formats use big-endian byte order with
 * zero-padding in upper bytes. Zero standard library dependencies.
 */

#include "sustfixed.h"

/* ============================================================================
 * Internal helper: zero-fill a buffer using pure pointer arithmetic
 * ============================================================================ */
static void sustfix_zero_fill(uint8_t* buf, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        buf[i] = 0;
    }
}

/* ============================================================================
 * Internal helper: write a 64-bit value in big-endian at end of buffer
 * The value occupies the last 8 bytes; preceding bytes are zero-padded.
 * ============================================================================ */
static void sustfix_write_be64(uint8_t* buf, size_t slot_bytes, sucs_ex_char_t val) {
    size_t i;
    /* Zero-fill the entire slot */
    sustfix_zero_fill(buf, slot_bytes);
    /* Write 64-bit value big-endian into the last 8 bytes */
    if (slot_bytes >= 8) {
        size_t base = slot_bytes - 8;
        for (i = 0; i < 8; i++) {
            buf[base + i] = (uint8_t)((val >> (56 - i * 8)) & 0xFFULL);
        }
    }
}

/* ============================================================================
 * Internal helper: read a 64-bit value in big-endian from end of buffer
 * ============================================================================ */
static sucs_ex_char_t sustfix_read_be64(const uint8_t* buf, size_t slot_bytes) {
    sucs_ex_char_t val = 0;
    size_t i;
    if (slot_bytes >= 8) {
        size_t base = slot_bytes - 8;
        for (i = 0; i < 8; i++) {
            val |= ((sucs_ex_char_t)buf[base + i]) << (56 - i * 8);
        }
    }
    return val;
}

/* ============================================================================
 * SUST-32 (4-byte fixed-width transport)
 * Can only transport codepoints 0x00000000 to 0xFFFFFFFF (32-bit range).
 * ============================================================================ */
size_t sust32_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size) {
    if (!out_buf || buf_size < SUST32_BYTES || !extsucs_is_valid(ex_cp)) {
        return 0;
    }
    /* SUST-32 can only represent 32-bit values */
    if (ex_cp > 0xFFFFFFFFULL) {
        return 0;
    }
    uint32_t v = (uint32_t)(ex_cp & 0xFFFFFFFFULL);
    out_buf[0] = (uint8_t)((v >> 24) & 0xFF);
    out_buf[1] = (uint8_t)((v >> 16) & 0xFF);
    out_buf[2] = (uint8_t)((v >> 8)  & 0xFF);
    out_buf[3] = (uint8_t)(v         & 0xFF);
    return SUST32_BYTES;
}

size_t sust32_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp) {
    if (!in_buf || !out_cp || buf_size < SUST32_BYTES) {
        return 0;
    }
    sucs_ex_char_t cp = ((sucs_ex_char_t)in_buf[0] << 24) |
                        ((sucs_ex_char_t)in_buf[1] << 16) |
                        ((sucs_ex_char_t)in_buf[2] << 8)  |
                        ((sucs_ex_char_t)in_buf[3]);
    if (!extsucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return SUST32_BYTES;
}

/* ============================================================================
 * SUST-64 (8-byte fixed-width transport)
 * Full 64-bit ExtSUCS range supported.
 * ============================================================================ */
size_t sust64_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size) {
    if (!out_buf || buf_size < SUST64_BYTES || !extsucs_is_valid(ex_cp)) {
        return 0;
    }
    sustfix_write_be64(out_buf, SUST64_BYTES, ex_cp);
    return SUST64_BYTES;
}

size_t sust64_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp) {
    if (!in_buf || !out_cp || buf_size < SUST64_BYTES) {
        return 0;
    }
    sucs_ex_char_t cp = sustfix_read_be64(in_buf, SUST64_BYTES);
    if (!extsucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return SUST64_BYTES;
}

/* ============================================================================
 * SUST-128 (16-byte zero-padded, big-endian vector register slot)
 * ============================================================================ */
size_t sust128_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size) {
    if (!out_buf || buf_size < SUST128_BYTES || !extsucs_is_valid(ex_cp)) {
        return 0;
    }
    sustfix_write_be64(out_buf, SUST128_BYTES, ex_cp);
    return SUST128_BYTES;
}

size_t sust128_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp) {
    if (!in_buf || !out_cp || buf_size < SUST128_BYTES) {
        return 0;
    }
    sucs_ex_char_t cp = sustfix_read_be64(in_buf, SUST128_BYTES);
    if (!extsucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return SUST128_BYTES;
}

/* ============================================================================
 * SUST-256 (32-byte zero-padded, big-endian vector register slot)
 * ============================================================================ */
size_t sust256_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size) {
    if (!out_buf || buf_size < SUST256_BYTES || !extsucs_is_valid(ex_cp)) {
        return 0;
    }
    sustfix_write_be64(out_buf, SUST256_BYTES, ex_cp);
    return SUST256_BYTES;
}

size_t sust256_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp) {
    if (!in_buf || !out_cp || buf_size < SUST256_BYTES) {
        return 0;
    }
    sucs_ex_char_t cp = sustfix_read_be64(in_buf, SUST256_BYTES);
    if (!extsucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return SUST256_BYTES;
}

/* ============================================================================
 * SUST-512 (64-byte zero-padded, big-endian vector register slot)
 * ============================================================================ */
size_t sust512_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t buf_size) {
    if (!out_buf || buf_size < SUST512_BYTES || !extsucs_is_valid(ex_cp)) {
        return 0;
    }
    sustfix_write_be64(out_buf, SUST512_BYTES, ex_cp);
    return SUST512_BYTES;
}

size_t sust512_decode(const uint8_t* in_buf, size_t buf_size, sucs_ex_char_t* out_cp) {
    if (!in_buf || !out_cp || buf_size < SUST512_BYTES) {
        return 0;
    }
    sucs_ex_char_t cp = sustfix_read_be64(in_buf, SUST512_BYTES);
    if (!extsucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return SUST512_BYTES;
}

/* ============================================================================
 * SUST-N (Arbitrary N-byte fixed-width transport)
 * Minimum slot_bytes is 8 for full 64-bit ExtSUCS coverage.
 * ============================================================================ */
size_t sustn_encode(sucs_ex_char_t ex_cp, uint8_t* out_buf, size_t slot_bytes) {
    if (!out_buf || slot_bytes < 8 || !extsucs_is_valid(ex_cp)) {
        return 0;
    }
    sustfix_write_be64(out_buf, slot_bytes, ex_cp);
    return slot_bytes;
}

size_t sustn_decode(const uint8_t* in_buf, size_t slot_bytes, sucs_ex_char_t* out_cp) {
    if (!in_buf || !out_cp || slot_bytes < 8) {
        return 0;
    }
    sucs_ex_char_t cp = sustfix_read_be64(in_buf, slot_bytes);
    if (!extsucs_is_valid(cp)) {
        return 0;
    }
    *out_cp = cp;
    return slot_bytes;
}