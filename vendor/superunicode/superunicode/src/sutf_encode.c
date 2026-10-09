#include "superunicode/sutf.h"

int sutf_encode_char(sucs_char_t cp, char* out_buf, size_t buf_size, size_t* out_bytes_written) {
    if (!out_buf || !out_bytes_written) {
        return SUES_ERR_INVALID_BYTE;
    }

    if (cp > SUCS_MAX_CODEPOINT) {
        return SUES_ERR_OUT_OF_BOUNDS;
    }

    /* Kernel Security Trap range and sentinel are not encodable in Base SUCS */
    if (cp >= SUCS_KERNEL_TRAP_MIN && cp <= SUCS_KERNEL_TRAP_MAX) {
        return SUES_ERR_INVALID_CODEPOINT;
    }
    if (cp == SUCS_INVALID_CODEPOINT) {
        return SUES_ERR_INVALID_CODEPOINT;
    }

    if (cp <= 0x7FUL) {
        if (buf_size < 1) return SUES_ERR_BUFFER_TOO_SMALL;
        out_buf[0] = (char)(cp & 0x7FUL);
        *out_bytes_written = 1;
    } else if (cp <= 0x7FFUL) {
        if (buf_size < 2) return SUES_ERR_BUFFER_TOO_SMALL;
        out_buf[0] = (char)(0xC0UL | ((cp >> 6) & 0x1FUL));
        out_buf[1] = (char)(0x80UL | (cp & 0x3FUL));
        *out_bytes_written = 2;
    } else if (cp <= 0xFFFFUL) {
        if (buf_size < 3) return SUES_ERR_BUFFER_TOO_SMALL;
        out_buf[0] = (char)(0xE0UL | ((cp >> 12) & 0x0FUL));
        out_buf[1] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
        out_buf[2] = (char)(0x80UL | (cp & 0x3FUL));
        *out_bytes_written = 3;
    } else if (cp <= 0x0010FFFFUL) {
        if (buf_size < 4) return SUES_ERR_BUFFER_TOO_SMALL;
        out_buf[0] = (char)(0xF0UL | ((cp >> 18) & 0x07UL));
        out_buf[1] = (char)(0x80UL | ((cp >> 12) & 0x3FUL));
        out_buf[2] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
        out_buf[3] = (char)(0x80UL | (cp & 0x3FUL));
        *out_bytes_written = 4;
    } else if (cp <= 0x03FFFFFFUL) {
        if (buf_size < 5) return SUES_ERR_BUFFER_TOO_SMALL;
        out_buf[0] = (char)(0xF8UL | ((cp >> 24) & 0x03UL));
        out_buf[1] = (char)(0x80UL | ((cp >> 18) & 0x3FUL));
        out_buf[2] = (char)(0x80UL | ((cp >> 12) & 0x3FUL));
        out_buf[3] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
        out_buf[4] = (char)(0x80UL | (cp & 0x3FUL));
        *out_bytes_written = 5;
    } else {
        if (buf_size < 6) return SUES_ERR_BUFFER_TOO_SMALL;
        out_buf[0] = (char)(0xFCUL | ((cp >> 30) & 0x01UL));
        out_buf[1] = (char)(0x80UL | ((cp >> 24) & 0x3FUL));
        out_buf[2] = (char)(0x80UL | ((cp >> 18) & 0x3FUL));
        out_buf[3] = (char)(0x80UL | ((cp >> 12) & 0x3FUL));
        out_buf[4] = (char)(0x80UL | ((cp >> 6) & 0x3FUL));
        out_buf[5] = (char)(0x80UL | (cp & 0x3FUL));
        *out_bytes_written = 6;
    }

    return SUES_SUCCESS;
}
