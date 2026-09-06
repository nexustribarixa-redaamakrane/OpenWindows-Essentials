/**
 * Plugin Integrity Checksums & Blob Utilities
 *
 * Freestanding C99 implementation of the plugin blob integrity layer:
 *   - CRC32c (Castagnoli, polynomial 0x82F63B78, reflected)
 *   - Fletcher-64 (32-bit words, sums modulo 0xFFFFFFFF)
 *   - Blob checksum computation / verification over the zeroed-checksum header
 *   - Range record parsing & validation
 *
 * Zero standard library dependencies.
 */

#include "superunicode_extended/plugin_checksum.h"

/* ============================================================================
 * Fletcher-64 Streaming State
 * ============================================================================ */
static void fletcher64_accum_word(sucs_fletcher64_state_t* state, uint32_t word) {
    state->sum1 = (state->sum1 + word) % 0xFFFFFFFFUL;
    state->sum2 = (state->sum2 + state->sum1) % 0xFFFFFFFFUL;
}

void sucs_fletcher64_init(sucs_fletcher64_state_t* state) {
    if (!state) return;
    state->sum1 = 0xFFFFFFFFUL;
    state->sum2 = 0xFFFFFFFFUL;
    state->pending = 0;
}

void sucs_fletcher64_update(sucs_fletcher64_state_t* state, const uint8_t* data, size_t len) {
    if (!state || !data) return;

    size_t i = 0;

    /* Finish any partially-accumulated word from a previous update. */
    if (state->pending != 0) {
        while (state->pending < 4 && i < len) {
            state->buf[state->pending] = data[i];
            state->pending++;
            i++;
        }
        if (state->pending == 4) {
            uint32_t word = ((uint32_t)state->buf[0]) |
                            ((uint32_t)state->buf[1] << 8)  |
                            ((uint32_t)state->buf[2] << 16) |
                            ((uint32_t)state->buf[3] << 24);
            fletcher64_accum_word(state, word);
            state->pending = 0;
        }
    }

    /* Consume full words in the fast path. */
    while (i + 4 <= len) {
        uint32_t word = ((uint32_t)data[i]) |
                        ((uint32_t)data[i + 1] << 8)  |
                        ((uint32_t)data[i + 2] << 16) |
                        ((uint32_t)data[i + 3] << 24);
        fletcher64_accum_word(state, word);
        i += 4;
    }

    /* Buffer the tail for the next update / final. */
    while (i < len) {
        state->buf[state->pending] = data[i];
        state->pending++;
        i++;
    }
}

uint64_t sucs_fletcher64_final(sucs_fletcher64_state_t* state) {
    if (!state) return 0;

    if (state->pending != 0) {
        uint32_t word = 0;
        for (uint8_t k = 0; k < state->pending; ++k) {
            word |= ((uint32_t)state->buf[k]) << (8 * k);
        }
        fletcher64_accum_word(state, word);
        state->pending = 0;
    }

    if (state->sum1 == 0) state->sum1 = 0xFFFFFFFFUL;
    if (state->sum2 == 0) state->sum2 = 0xFFFFFFFFUL;
    return ((uint64_t)state->sum2 << 32) | state->sum1;
}

/* ============================================================================
 * CRC32c (Castagnoli)
 * ============================================================================ */
uint32_t sucs_checksum_crc32c_update(uint32_t crc, const uint8_t* data, size_t len) {
    if (!data) return crc;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1U);
            crc = (crc >> 1) ^ (0x82F63B78UL & mask);
        }
    }
    return crc;
}

uint32_t sucs_checksum_crc32c(const uint8_t* data, size_t len) {
    return sucs_checksum_crc32c_update(0xFFFFFFFFUL, data, len) ^ 0xFFFFFFFFUL;
}

uint64_t sucs_checksum_fletcher64(const uint8_t* data, size_t len) {
    sucs_fletcher64_state_t state;
    sucs_fletcher64_init(&state);
    sucs_fletcher64_update(&state, data, len);
    return sucs_fletcher64_final(&state);
}

/* ============================================================================
 * Plugin Blob Checksum Computation & Verification
 *
 * Checksums cover the ENTIRE blob with the header's crc32c and fletcher64
 * fields zeroed. A zeroed copy of the header is computed in place so the
 * input blob is never modified.
 * ============================================================================ */
sucs_plugin_status_t sucs_plugin_compute_checksums(const uint8_t* blob,
                                                   size_t blob_size,
                                                   uint32_t* out_crc,
                                                   uint64_t* out_fletcher) {
    if (!blob || !out_crc || !out_fletcher ||
        blob_size < sizeof(sucs_plugin_blob_header_t)) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }

    const sucs_plugin_blob_header_t* hdr = (const sucs_plugin_blob_header_t*)blob;
    if (hdr->magic != SUCS_PLUGIN_BLOB_MAGIC) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }
    if (hdr->blob_version != SUCS_PLUGIN_BLOB_VERSION) {
        return SUCS_PLUGIN_ERR_UNSUPPORTED_VERSION;
    }
    if ((size_t)hdr->blob_size != blob_size) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }

    /* Zeroed header copy. */
    uint8_t hbuf[sizeof(sucs_plugin_blob_header_t)];
    for (size_t i = 0; i < sizeof(hbuf); ++i) {
        hbuf[i] = blob[i];
    }
    size_t off_crc = offsetof(sucs_plugin_blob_header_t, crc32c);
    size_t off_f64 = offsetof(sucs_plugin_blob_header_t, fletcher64);
    for (size_t i = 0; i < 4; ++i) hbuf[off_crc + i] = 0;
    for (size_t i = 0; i < 8; ++i) hbuf[off_f64 + i] = 0;

    /* CRC32c over header part, then payload part. */
    uint32_t crc = sucs_checksum_crc32c_update(0xFFFFFFFFUL, hbuf, sizeof(hbuf));
    crc = sucs_checksum_crc32c_update(crc, blob + sizeof(hbuf), blob_size - sizeof(hbuf));
    *out_crc = crc ^ 0xFFFFFFFFUL;

    /* Fletcher-64 over header part, then payload part. */
    sucs_fletcher64_state_t fstate;
    sucs_fletcher64_init(&fstate);
    sucs_fletcher64_update(&fstate, hbuf, sizeof(hbuf));
    sucs_fletcher64_update(&fstate, blob + sizeof(hbuf), blob_size - sizeof(hbuf));
    *out_fletcher = sucs_fletcher64_final(&fstate);

    return SUCS_PLUGIN_OK;
}

bool sucs_plugin_blob_verify(const uint8_t* blob, size_t blob_size) {
    uint32_t crc = 0;
    uint64_t fletcher = 0;
    if (sucs_plugin_compute_checksums(blob, blob_size, &crc, &fletcher) != SUCS_PLUGIN_OK) {
        return false;
    }
    const sucs_plugin_blob_header_t* hdr = (const sucs_plugin_blob_header_t*)blob;
    return (hdr->crc32c == crc) && (hdr->fletcher64 == fletcher);
}

/* ============================================================================
 * Range Record Parsing (16-byte little-endian pairs after the header)
 * ============================================================================ */
sucs_plugin_status_t sucs_plugin_parse_ranges(const uint8_t* blob,
                                              size_t blob_size,
                                              sucs_plugin_range_t* out,
                                              uint32_t max_ranges,
                                              uint32_t* out_count) {
    if (!blob || !out || blob_size < sizeof(sucs_plugin_blob_header_t)) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }

    const sucs_plugin_blob_header_t* hdr = (const sucs_plugin_blob_header_t*)blob;
    if (hdr->magic != SUCS_PLUGIN_BLOB_MAGIC) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }
    if ((size_t)hdr->blob_size != blob_size) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }
    if (hdr->range_count == 0 || hdr->range_count > max_ranges) {
        return SUCS_PLUGIN_ERR_INVALID_RANGE;
    }

    size_t need = sizeof(sucs_plugin_blob_header_t) +
                  (size_t)hdr->range_count * sizeof(sucs_plugin_range_t);
    if (need > blob_size) {
        return SUCS_PLUGIN_ERR_INVALID_BLOB;
    }

    const uint8_t* p = blob + sizeof(sucs_plugin_blob_header_t);
    for (uint32_t i = 0; i < hdr->range_count; ++i) {
        sucs_ex_char_t start = 0;
        sucs_ex_char_t end = 0;
        for (int b = 0; b < 8; ++b) {
            start |= ((sucs_ex_char_t)p[i * 16 + b]) << (8 * b);
            end   |= ((sucs_ex_char_t)p[i * 16 + 8 + b]) << (8 * b);
        }
        out[i].start = start;
        out[i].end = end;
    }

    if (out_count) {
        *out_count = hdr->range_count;
    }
    return SUCS_PLUGIN_OK;
}

/* ============================================================================
 * Range Validation (must extend past the base limit)
 * ============================================================================ */
sucs_plugin_status_t sucs_plugin_validate_ranges(const sucs_plugin_range_t* ranges,
                                                 uint32_t count) {
    if (!ranges || count == 0) {
        return SUCS_PLUGIN_ERR_INVALID_RANGE;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (ranges[i].start > ranges[i].end) {
            return SUCS_PLUGIN_ERR_INVALID_RANGE;
        }
        if (!extsucs_is_valid(ranges[i].start) || !extsucs_is_valid(ranges[i].end)) {
            return SUCS_PLUGIN_ERR_INVALID_RANGE;
        }
        /* A plugin's sole purpose is adding codepoints past the base limit. */
        if (ranges[i].start <= SUCS_PLUGIN_BASE_LIMIT) {
            return SUCS_PLUGIN_ERR_RANGE_BELOW_BASE;
        }
    }
    return SUCS_PLUGIN_OK;
}
