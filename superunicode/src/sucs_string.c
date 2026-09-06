#include "superunicode/superunicode.h"

int sucs_strlen(const SUCS_STRING* str, size_t* out_char_count) {
    if (!str || !str->buffer || !out_char_count) {
        return SUES_ERR_INVALID_BYTE;
    }

    size_t count = 0;
    size_t offset = 0;

    while (offset < str->length_bytes) {
        sucs_char_t cp = 0;
        size_t bytes_read = 0;
        int status = sutf_decode_char(str->buffer + offset, str->length_bytes - offset, &cp, &bytes_read);
        if (status != SUES_SUCCESS) {
            return status;
        }

        /* Skip System Control Plane (SUCS_TYPE_SYS_FUNCTION) formatting codes in visual string length */
        if (!sucs_is_scp_plane(cp)) {
            count++;
        }

        offset += bytes_read;
    }

    *out_char_count = count;
    return SUES_SUCCESS;
}

int sucs_codepoint_count(const SUCS_STRING* str, size_t* out_cp_count) {
    if (!str || !str->buffer || !out_cp_count) {
        return SUES_ERR_INVALID_BYTE;
    }

    size_t count = 0;
    size_t offset = 0;

    while (offset < str->length_bytes) {
        sucs_char_t cp = 0;
        size_t bytes_read = 0;
        int status = sutf_decode_char(str->buffer + offset, str->length_bytes - offset, &cp, &bytes_read);
        if (status != SUES_SUCCESS) {
            return status;
        }

        count++;
        offset += bytes_read;
    }

    *out_cp_count = count;
    return SUES_SUCCESS;
}

int sucs_strcpy(SUCS_STRING* dest, const SUCS_STRING* src) {
    if (!dest || !dest->buffer || !src || !src->buffer) {
        return SUES_ERR_INVALID_BYTE;
    }

    if (dest->capacity_bytes < src->length_bytes) {
        return SUES_ERR_BUFFER_TOO_SMALL;
    }

    for (uint32_t i = 0; i < src->length_bytes; i++) {
        dest->buffer[i] = src->buffer[i];
    }
    dest->length_bytes = src->length_bytes;

    return SUES_SUCCESS;
}

int sucs_streq(const SUCS_STRING* a, const SUCS_STRING* b, bool* out_equal) {
    if (!a || !a->buffer || !b || !b->buffer || !out_equal) {
        return SUES_ERR_INVALID_BYTE;
    }

    if (a->length_bytes != b->length_bytes) {
        *out_equal = false;
        return SUES_SUCCESS;
    }

    for (uint32_t i = 0; i < a->length_bytes; i++) {
        if (a->buffer[i] != b->buffer[i]) {
            *out_equal = false;
            return SUES_SUCCESS;
        }
    }

    *out_equal = true;
    return SUES_SUCCESS;
}
