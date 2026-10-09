/**
 * @file suf_parser.c
 * @brief Zero-allocation Freestanding Parser Implementation for .suf Files
 */

#include "suf/suf_parser.h"
#include <string.h>

static inline bool suf_bounds_check(size_t total_size, uint32_t offset, uint32_t length) {
    if ((uint64_t)offset + (uint64_t)length > (uint64_t)total_size) {
        return false;
    }
    return true;
}

suf_status_t suf_validate_header(const void *buffer, size_t size, suf_header_t *out_header) {
    if (!buffer) return SUF_ERR_NULL_POINTER;
    if (size < sizeof(suf_header_t)) return SUF_ERR_BUFFER_TOO_SMALL;

    const suf_header_t *hdr = (const suf_header_t *)buffer;
    if (hdr->magic != SUF_MAGIC) return SUF_ERR_INVALID_MAGIC;
    if (hdr->version != SUF_VERSION_CURRENT) return SUF_ERR_INVALID_VERSION;

    /* Validate table bounds */
    if (hdr->cmap_size > 0 && !suf_bounds_check(size, hdr->cmap_offset, hdr->cmap_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->metrics_size > 0 && !suf_bounds_check(size, hdr->metrics_offset, hdr->metrics_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->boot_bitmap_size > 0 && !suf_bounds_check(size, hdr->boot_bitmap_offset, hdr->boot_bitmap_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->outlines_size > 0 && !suf_bounds_check(size, hdr->outlines_offset, hdr->outlines_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->kerning_size > 0 && !suf_bounds_check(size, hdr->kerning_offset, hdr->kerning_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->ligatures_size > 0 && !suf_bounds_check(size, hdr->ligatures_offset, hdr->ligatures_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->var_axes_size > 0 && !suf_bounds_check(size, hdr->var_axes_offset, hdr->var_axes_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->plugin_meta_size > 0 && !suf_bounds_check(size, hdr->plugin_meta_offset, hdr->plugin_meta_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->gvar_size > 0 && !suf_bounds_check(size, hdr->gvar_offset, hdr->gvar_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }
    if (hdr->names_size > 0 && !suf_bounds_check(size, hdr->names_offset, hdr->names_size)) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }

    /* Format rule: mode flags must reflect serialized content. A header that
     * claims a rendering mode without the corresponding table is malformed. */
    if ((hdr->flags & SUF_FLAG_BOOT_BITMAP) && hdr->boot_bitmap_size == 0) {
        return SUF_ERR_INVALID_HEADER;
    }
    if ((hdr->flags & SUF_FLAG_OS_VECTOR) && hdr->outlines_size == 0) {
        return SUF_ERR_INVALID_HEADER;
    }
    if ((hdr->flags & SUF_FLAG_GLYPH_VARIATIONS) && hdr->gvar_size == 0) {
        return SUF_ERR_INVALID_HEADER;
    }

    if (out_header) {
        *out_header = *hdr;
    }
    return SUF_OK;
}

suf_status_t suf_lookup_glyph_id(const void *buffer, size_t size, uint32_t codepoint, uint32_t *out_glyph_id) {
    if (!buffer || !out_glyph_id) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    if (hdr.flags & SUF_FLAG_EXTSUCS) {
        return suf_lookup_glyph_id_ext(buffer, size, (uint64_t)codepoint, out_glyph_id);
    }

    if (hdr.cmap_size == 0 || hdr.cmap_offset == 0) {
        return SUF_ERR_GLYPH_NOT_FOUND;
    }

    size_t count = hdr.cmap_size / sizeof(suf_cmap_entry_t);
    const suf_cmap_entry_t *entries = (const suf_cmap_entry_t *)((const uint8_t *)buffer + hdr.cmap_offset);

    /* Binary search */
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (entries[mid].codepoint == codepoint) {
            *out_glyph_id = entries[mid].glyph_id;
            return SUF_OK;
        } else if (entries[mid].codepoint < codepoint) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return SUF_ERR_GLYPH_NOT_FOUND;
}

suf_status_t suf_lookup_glyph_id_ext(const void *buffer, size_t size, uint64_t codepoint, uint32_t *out_glyph_id) {
    if (!buffer || !out_glyph_id) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    if (!(hdr.flags & SUF_FLAG_EXTSUCS)) {
        if (codepoint > 0x7FFFFFFFUL) return SUF_ERR_GLYPH_NOT_FOUND;
        return suf_lookup_glyph_id(buffer, size, (uint32_t)codepoint, out_glyph_id);
    }

    if (hdr.cmap_size == 0 || hdr.cmap_offset == 0) {
        return SUF_ERR_GLYPH_NOT_FOUND;
    }

    size_t count = hdr.cmap_size / sizeof(suf_cmap_ext_entry_t);
    const suf_cmap_ext_entry_t *entries = (const suf_cmap_ext_entry_t *)((const uint8_t *)buffer + hdr.cmap_offset);

    /* Binary search */
    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (entries[mid].codepoint == codepoint) {
            *out_glyph_id = entries[mid].glyph_id;
            return SUF_OK;
        } else if (entries[mid].codepoint < codepoint) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return SUF_ERR_GLYPH_NOT_FOUND;
}

suf_status_t suf_get_glyph_metric(const void *buffer, size_t size, uint32_t glyph_id, suf_metric_t *out_metric) {
    if (!buffer || !out_metric) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    if (glyph_id >= hdr.glyph_count) return SUF_ERR_GLYPH_NOT_FOUND;
    if (hdr.metrics_size < (glyph_id + 1) * sizeof(suf_metric_t)) return SUF_ERR_OUT_OF_BOUNDS;

    const suf_metric_t *metrics = (const suf_metric_t *)((const uint8_t *)buffer + hdr.metrics_offset);
    *out_metric = metrics[glyph_id];
    return SUF_OK;
}

suf_status_t suf_get_boot_bitmap(const void *buffer, size_t size, uint32_t glyph_id,
                                 const uint8_t **out_bitmap_bytes, size_t *out_byte_count) {
    if (!buffer || !out_bitmap_bytes || !out_byte_count) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    if (!(hdr.flags & SUF_FLAG_BOOT_BITMAP) || hdr.boot_bitmap_size == 0) {
        return SUF_ERR_NO_BITMAP;
    }
    if (glyph_id >= hdr.glyph_count) return SUF_ERR_GLYPH_NOT_FOUND;

    uint32_t row_bytes = (hdr.boot_bitmap_bpp == 1) ? ((hdr.boot_bitmap_width + 7) / 8) : (uint32_t)hdr.boot_bitmap_width;
    uint32_t bytes_per_glyph = row_bytes * hdr.boot_bitmap_height;

    uint32_t glyph_offset = glyph_id * bytes_per_glyph;
    if (glyph_offset + bytes_per_glyph > hdr.boot_bitmap_size) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }

    *out_bitmap_bytes = (const uint8_t *)buffer + hdr.boot_bitmap_offset + glyph_offset;
    *out_byte_count = bytes_per_glyph;
    return SUF_OK;
}

suf_status_t suf_get_glyph_outline(const void *buffer, size_t size, uint32_t glyph_id,
                                   const uint8_t **out_commands, size_t *out_cmd_size) {
    if (!buffer || !out_commands || !out_cmd_size) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    if (!(hdr.flags & SUF_FLAG_OS_VECTOR) || hdr.outlines_size == 0) {
        return SUF_ERR_NO_OUTLINE;
    }
    if (glyph_id >= hdr.glyph_count) return SUF_ERR_GLYPH_NOT_FOUND;

    suf_metric_t metric;
    st = suf_get_glyph_metric(buffer, size, glyph_id, &metric);
    if (st != SUF_OK) return st;

    uint32_t offset = metric.data_offset;
    if (offset >= hdr.outlines_size) return SUF_ERR_OUT_OF_BOUNDS;

    const uint8_t *stream = (const uint8_t *)buffer + hdr.outlines_offset + offset;
    const uint8_t *stream_end = (const uint8_t *)buffer + hdr.outlines_offset + hdr.outlines_size;

    if (stream + 2 > stream_end) return SUF_ERR_OUT_OF_BOUNDS;
    uint16_t len = (uint16_t)(stream[0] | (stream[1] << 8));
    if (stream + 2 + len > stream_end) return SUF_ERR_OUT_OF_BOUNDS;

    *out_commands = stream + 2;
    *out_cmd_size = len;
    return SUF_OK;
}

suf_status_t suf_get_glyph_variation(const void *buffer, size_t size, uint32_t glyph_id,
                                     const uint8_t **out_data, size_t *out_byte_count) {
    if (!buffer || !out_data || !out_byte_count) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    *out_data = NULL;
    *out_byte_count = 0;

    if (!(hdr.flags & SUF_FLAG_GLYPH_VARIATIONS) || hdr.gvar_size == 0) {
        return SUF_ERR_CORRUPT_DATA;
    }
    if (glyph_id >= hdr.glyph_count) return SUF_ERR_GLYPH_NOT_FOUND;
    if (hdr.gvar_size < 8) return SUF_ERR_OUT_OF_BOUNDS;

    const uint8_t *blob = (const uint8_t *)buffer + hdr.gvar_offset;
    uint32_t magic, count;
    memcpy(&magic, blob, 4);
    memcpy(&count, blob + 4, 4);
    if (magic != SUF_GVAR_BLOB_MAGIC) return SUF_ERR_INVALID_MAGIC;
    if (glyph_id >= count) return SUF_ERR_GLYPH_NOT_FOUND;

    const uint32_t *sizes = (const uint32_t *)(const void *)(blob + 8);
    size_t prefix = 0;
    for (uint32_t i = 0; i < glyph_id; ++i) {
        prefix += sizes[i];
    }

    size_t blocks_start = 8 + (size_t)count * 4;
    if (prefix + sizes[glyph_id] > hdr.gvar_size - blocks_start) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }

    const uint8_t *block = blob + blocks_start + prefix;
    if (!suf_bounds_check(size, hdr.gvar_offset + blocks_start + (uint32_t)prefix,
                          (uint32_t)sizes[glyph_id])) {
        return SUF_ERR_OUT_OF_BOUNDS;
    }

    *out_data = block;
    *out_byte_count = sizes[glyph_id];
    return SUF_OK;
}

suf_status_t suf_get_name_count(const void *buffer, size_t size, uint32_t *out_count) {
    if (!buffer || !out_count) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    *out_count = 0;
    if (hdr.names_size < 8) return SUF_OK;

    const uint8_t *blob = (const uint8_t *)buffer + hdr.names_offset;
    uint32_t magic, count;
    memcpy(&magic, blob, 4);
    memcpy(&count, blob + 4, 4);
    if (magic != SUF_NAMES_BLOB_MAGIC) return SUF_ERR_INVALID_MAGIC;
    if (count > SUF_NAMES_MAX_RECORDS) return SUF_ERR_INVALID_HEADER;

    *out_count = count;
    return SUF_OK;
}

suf_status_t suf_get_name(const void *buffer, size_t size, uint16_t name_id,
                          const char **out_utf8, size_t *out_len) {
    if (!buffer || !out_utf8 || !out_len) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    *out_utf8 = NULL;
    *out_len = 0;

    if (hdr.names_size < 8) return SUF_ERR_CORRUPT_DATA;

    const uint8_t *blob = (const uint8_t *)buffer + hdr.names_offset;
    uint32_t magic, count;
    memcpy(&magic, blob, 4);
    memcpy(&count, blob + 4, 4);
    if (magic != SUF_NAMES_BLOB_MAGIC) return SUF_ERR_INVALID_MAGIC;
    if (count > SUF_NAMES_MAX_RECORDS) return SUF_ERR_INVALID_HEADER;

    size_t pos = 8;
    for (uint32_t i = 0; i < count; ++i) {
        if (pos + 4 > hdr.names_size) return SUF_ERR_OUT_OF_BOUNDS;
        uint16_t id = (uint16_t)(blob[pos] | ((uint16_t)blob[pos + 1] << 8));
        uint16_t len = (uint16_t)(blob[pos + 2] | ((uint16_t)blob[pos + 3] << 8));
        pos += 4;
        if (pos + len > hdr.names_size) return SUF_ERR_OUT_OF_BOUNDS;
        if (id == name_id) {
            *out_utf8 = (const char *)(blob + pos);
            *out_len = len;
            return SUF_OK;
        }
        pos += len;
    }
    return SUF_ERR_CORRUPT_DATA;
}

int16_t suf_get_kerning(const void *buffer, size_t size, uint32_t left_glyph, uint32_t right_glyph) {
    if (!buffer) return 0;
    suf_header_t hdr;
    if (suf_validate_header(buffer, size, &hdr) != SUF_OK) return 0;
    if (!(hdr.flags & SUF_FLAG_KERNING) || hdr.kerning_size == 0) return 0;

    size_t count = hdr.kerning_size / sizeof(suf_kern_pair_t);
    const suf_kern_pair_t *pairs = (const suf_kern_pair_t *)((const uint8_t *)buffer + hdr.kerning_offset);

    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (pairs[mid].left_glyph == left_glyph) {
            if (pairs[mid].right_glyph == right_glyph) {
                return pairs[mid].kerning;
            } else if (pairs[mid].right_glyph < right_glyph) {
                low = mid + 1;
            } else {
                high = mid;
            }
        } else if (pairs[mid].left_glyph < left_glyph) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return 0;
}

uint32_t suf_lookup_ligature(const void *buffer, size_t size, uint32_t first_glyph, uint32_t second_glyph) {
    if (!buffer) return 0;
    suf_header_t hdr;
    if (suf_validate_header(buffer, size, &hdr) != SUF_OK) return 0;
    if (!(hdr.flags & SUF_FLAG_LIGATURES) || hdr.ligatures_size == 0) return 0;

    size_t count = hdr.ligatures_size / sizeof(suf_ligature_t);
    const suf_ligature_t *ligs = (const suf_ligature_t *)((const uint8_t *)buffer + hdr.ligatures_offset);

    size_t low = 0;
    size_t high = count;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        if (ligs[mid].first_glyph == first_glyph) {
            if (ligs[mid].second_glyph == second_glyph) {
                return ligs[mid].replacement_glyph;
            } else if (ligs[mid].second_glyph < second_glyph) {
                low = mid + 1;
            } else {
                high = mid;
            }
        } else if (ligs[mid].first_glyph < first_glyph) {
            low = mid + 1;
        } else {
            high = mid;
        }
    }

    return 0;
}

suf_status_t suf_render_boot_glyph_to_fb(const void *buffer, size_t size, uint32_t glyph_id,
                                        uint32_t *fb, uint32_t fb_width, uint32_t fb_height,
                                        uint32_t fb_pitch_pixels, uint32_t x, uint32_t y,
                                        uint32_t fg_color, uint32_t bg_color) {
    if (!buffer || !fb) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    const uint8_t *bitmap = NULL;
    size_t byte_count = 0;
    st = suf_get_boot_bitmap(buffer, size, glyph_id, &bitmap, &byte_count);
    if (st != SUF_OK) return st;

    uint32_t gw = hdr.boot_bitmap_width;
    uint32_t gh = hdr.boot_bitmap_height;
    bool has_bg = (bg_color != 0);

    if (hdr.boot_bitmap_bpp == 1) {
        uint32_t row_bytes = (gw + 7) / 8;
        for (uint32_t r = 0; r < gh; ++r) {
            uint32_t dest_y = y + r;
            if (dest_y >= fb_height) break;

            const uint8_t *row_ptr = bitmap + (r * row_bytes);
            uint32_t *dest_row = fb + (dest_y * fb_pitch_pixels);

            for (uint32_t c = 0; c < gw; ++c) {
                uint32_t dest_x = x + c;
                if (dest_x >= fb_width) break;

                uint8_t byte = row_ptr[c / 8];
                bool bit_set = (byte & (0x80 >> (c % 8))) != 0;

                if (bit_set) {
                    dest_row[dest_x] = fg_color;
                } else if (has_bg) {
                    dest_row[dest_x] = bg_color;
                }
            }
        }
    } else if (hdr.boot_bitmap_bpp == 8) {
        for (uint32_t r = 0; r < gh; ++r) {
            uint32_t dest_y = y + r;
            if (dest_y >= fb_height) break;

            const uint8_t *row_ptr = bitmap + (r * gw);
            uint32_t *dest_row = fb + (dest_y * fb_pitch_pixels);

            for (uint32_t c = 0; c < gw; ++c) {
                uint32_t dest_x = x + c;
                if (dest_x >= fb_width) break;

                uint8_t alpha = row_ptr[c];
                if (alpha == 255) {
                    dest_row[dest_x] = fg_color;
                } else if (alpha == 0) {
                    if (has_bg) dest_row[dest_x] = bg_color;
                } else {
                    uint32_t base = has_bg ? bg_color : dest_row[dest_x];
                    uint32_t f_r = (fg_color >> 16) & 0xFF;
                    uint32_t f_g = (fg_color >> 8) & 0xFF;
                    uint32_t f_b = fg_color & 0xFF;
                    uint32_t b_r = (base >> 16) & 0xFF;
                    uint32_t b_g = (base >> 8) & 0xFF;
                    uint32_t b_b = base & 0xFF;

                    uint32_t out_r = (f_r * alpha + b_r * (255 - alpha)) / 255;
                    uint32_t out_g = (f_g * alpha + b_g * (255 - alpha)) / 255;
                    uint32_t out_b = (f_b * alpha + b_b * (255 - alpha)) / 255;

                    dest_row[dest_x] = 0xFF000000UL | (out_r << 16) | (out_g << 8) | out_b;
                }
            }
        }
    }

    return SUF_OK;
}

/* ========================================================================= */
/* BANcode & Kernel Crash Diagnostics Extensions                             */
/* ========================================================================= */

suf_status_t suf_lookup_bancode_glyph(const void *buffer, size_t size, uint32_t bancode_cp, uint32_t *out_glyph_id) {
    if (!buffer || !out_glyph_id) return SUF_ERR_NULL_POINTER;

    /* Direct lookup first */
    suf_status_t st = suf_lookup_glyph_id(buffer, size, bancode_cp, out_glyph_id);
    if (st == SUF_OK) return SUF_OK;

    /* Fallback to class base sentinel glyph */
    uint32_t fallback_cp = 0;
    if (bancode_cp >= SUF_BANCODE_FATAL_MIN && bancode_cp <= SUF_BANCODE_FATAL_MAX) {
        fallback_cp = SUF_BANCODE_FATAL_MIN; /* B+ default */
    } else if (bancode_cp >= SUF_BANCODE_WARN_MIN && bancode_cp <= SUF_BANCODE_WARN_MAX) {
        fallback_cp = SUF_BANCODE_WARN_MIN;  /* W+ default */
    } else if (bancode_cp >= SUF_BANCODE_COM_MIN && bancode_cp <= SUF_BANCODE_COM_MAX) {
        fallback_cp = SUF_BANCODE_COM_MIN;   /* C+ default */
    } else if (bancode_cp >= SUF_BANCODE_SOFT_MIN && bancode_cp <= SUF_BANCODE_SOFT_MAX) {
        fallback_cp = SUF_BANCODE_SOFT_MIN;  /* S+ default */
    } else if (bancode_cp >= SUF_KERNEL_TRAP_MIN && bancode_cp <= SUF_KERNEL_TRAP_MAX) {
        fallback_cp = SUF_KERNEL_TRAP_MIN;   /* Trap default */
    }

    if (fallback_cp != 0) {
        st = suf_lookup_glyph_id(buffer, size, fallback_cp, out_glyph_id);
        if (st == SUF_OK) return SUF_OK;
    }

    /* Fallback to glyph 0 (.notdef) */
    *out_glyph_id = 0;
    return SUF_OK;
}

suf_status_t suf_render_bancode_badge(const void *buffer, size_t size, uint32_t bancode_cp,
                                     uint32_t *fb, uint32_t fb_width, uint32_t fb_height,
                                     uint32_t fb_pitch_pixels, uint32_t x, uint32_t y) {
    if (!buffer || !fb) return SUF_ERR_NULL_POINTER;

    uint32_t fg_color = 0xFFFFFFFFUL;
    uint32_t bg_color = 0xFF202020UL;
    uint32_t border_color = 0xFF808080UL;

    if (bancode_cp >= SUF_BANCODE_FATAL_MIN && bancode_cp <= SUF_BANCODE_FATAL_MAX) {
        fg_color = 0xFFFF4040UL; /* Bright Red */
        bg_color = 0xFF400808UL; /* Dark Crimson */
        border_color = 0xFFFF2020UL;
    } else if (bancode_cp >= SUF_BANCODE_WARN_MIN && bancode_cp <= SUF_BANCODE_WARN_MAX) {
        fg_color = 0xFFFFD020UL; /* Amber */
        bg_color = 0xFF403004UL;
        border_color = 0xFFFFBF00UL;
    } else if (bancode_cp >= SUF_BANCODE_COM_MIN && bancode_cp <= SUF_BANCODE_COM_MAX) {
        fg_color = 0xFF40FF90UL; /* Emerald Green */
        bg_color = 0xFF043818UL;
        border_color = 0xFF20DF80UL;
    } else if (bancode_cp >= SUF_BANCODE_SOFT_MIN && bancode_cp <= SUF_BANCODE_SOFT_MAX) {
        fg_color = 0xFF40E0FFUL; /* Cyan */
        bg_color = 0xFF042438UL;
        border_color = 0xFF20C0FFUL;
    } else if (bancode_cp >= SUF_KERNEL_TRAP_MIN && bancode_cp <= SUF_KERNEL_TRAP_MAX) {
        fg_color = 0xFFFF20A0UL; /* Neon Magenta */
        bg_color = 0xFF380020UL;
        border_color = 0xFFFF0080UL;
    }

    /* Draw 24x18 badge outline container */
    uint32_t badge_w = 24, badge_h = 18;
    for (uint32_t r = 0; r < badge_h; ++r) {
        uint32_t py = y + r;
        if (py >= fb_height) break;
        uint32_t *row = fb + py * fb_pitch_pixels;
        for (uint32_t c = 0; c < badge_w; ++c) {
            uint32_t px = x + c;
            if (px >= fb_width) break;

            if (r == 0 || r == badge_h - 1 || c == 0 || c == badge_w - 1) {
                row[px] = border_color;
            } else {
                row[px] = bg_color;
            }
        }
    }

    /* Render inner glyph if available */
    uint32_t gid = 0;
    if (suf_lookup_bancode_glyph(buffer, size, bancode_cp, &gid) == SUF_OK) {
        suf_render_boot_glyph_to_fb(buffer, size, gid, fb, fb_width, fb_height, fb_pitch_pixels,
                                   x + 4, y + 1, fg_color, 0);
    }

    return SUF_OK;
}

/* ========================================================================= */
/* Variable Axes & Interpolation Extensions                                  */
/* ========================================================================= */

suf_status_t suf_get_axis_count(const void *buffer, size_t size, uint32_t *out_count) {
    if (!buffer || !out_count) return SUF_ERR_NULL_POINTER;
    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    if (!(hdr.flags & SUF_FLAG_VARIABLE) || hdr.var_axes_size == 0) {
        *out_count = 0;
        return SUF_OK;
    }

    *out_count = hdr.var_axes_size / (uint32_t)sizeof(suf_var_axis_t);
    return SUF_OK;
}

suf_status_t suf_get_axis_info(const void *buffer, size_t size, uint32_t axis_index, suf_var_axis_t *out_axis) {
    if (!buffer || !out_axis) return SUF_ERR_NULL_POINTER;
    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    uint32_t count = hdr.var_axes_size / (uint32_t)sizeof(suf_var_axis_t);
    if (axis_index >= count) return SUF_ERR_AXIS_NOT_FOUND;

    const suf_var_axis_t *axes = (const suf_var_axis_t *)((const uint8_t *)buffer + hdr.var_axes_offset);
    *out_axis = axes[axis_index];
    return SUF_OK;
}

suf_status_t suf_find_axis_by_tag(const void *buffer, size_t size, uint32_t axis_tag, suf_var_axis_t *out_axis) {
    if (!buffer || !out_axis) return SUF_ERR_NULL_POINTER;
    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    uint32_t count = hdr.var_axes_size / (uint32_t)sizeof(suf_var_axis_t);
    const suf_var_axis_t *axes = (const suf_var_axis_t *)((const uint8_t *)buffer + hdr.var_axes_offset);

    for (uint32_t i = 0; i < count; ++i) {
        if (axes[i].tag == axis_tag) {
            *out_axis = axes[i];
            return SUF_OK;
        }
    }
    return SUF_ERR_AXIS_NOT_FOUND;
}

suf_status_t suf_interpolate_glyph_metric(const void *buffer, size_t size, uint32_t glyph_id,
                                         const float *axis_values, uint32_t num_axes,
                                         suf_metric_t *out_metric) {
    if (!buffer || !out_metric) return SUF_ERR_NULL_POINTER;

    suf_metric_t base_m;
    suf_status_t st = suf_get_glyph_metric(buffer, size, glyph_id, &base_m);
    if (st != SUF_OK) return st;

    if (!axis_values || num_axes == 0) {
        *out_metric = base_m;
        return SUF_OK;
    }

    uint32_t axis_count = 0;
    suf_get_axis_count(buffer, size, &axis_count);

    float width_scale = 1.0f;
    float weight_delta = 0.0f;
    float slant_factor = 0.0f;

    for (uint32_t i = 0; i < num_axes && i < axis_count; ++i) {
        suf_var_axis_t ax;
        if (suf_get_axis_info(buffer, size, i, &ax) == SUF_OK) {
            float val = axis_values[i];
            if (val < ax.min_val) val = ax.min_val;
            if (val > ax.max_val) val = ax.max_val;

            if (ax.tag == SUF_AXIS_WDTH) {
                width_scale = val / (ax.def_val > 0.0f ? ax.def_val : 100.0f);
            } else if (ax.tag == SUF_AXIS_WGHT) {
                weight_delta = (val - ax.def_val) / (ax.max_val - ax.min_val > 0.0f ? (ax.max_val - ax.min_val) : 800.0f);
            } else if (ax.tag == SUF_AXIS_SLNT) {
                slant_factor = val;
            }
        }
    }

    *out_metric = base_m;
    out_metric->advance_width = (int16_t)(base_m.advance_width * width_scale + weight_delta * 40.0f);
    out_metric->x_min = (int16_t)(base_m.x_min * width_scale - weight_delta * 10.0f);
    out_metric->x_max = (int16_t)(base_m.x_max * width_scale + weight_delta * 10.0f);

    (void)slant_factor;
    return SUF_OK;
}

/* ========================================================================= */
/* Modded SuperUnicode Plugin Font Extensions                                */
/* ========================================================================= */

suf_status_t suf_get_plugin_meta(const void *buffer, size_t size, suf_plugin_font_meta_t *out_meta) {
    if (!buffer || !out_meta) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buffer, size, &hdr);
    if (st != SUF_OK) return st;

    if (!(hdr.flags & SUF_FLAG_PLUGIN_FONT) || hdr.plugin_meta_size < sizeof(suf_plugin_font_meta_t)) {
        return SUF_ERR_INVALID_PLUGIN;
    }

    const suf_plugin_font_meta_t *meta = (const suf_plugin_font_meta_t *)((const uint8_t *)buffer + hdr.plugin_meta_offset);
    *out_meta = *meta;
    return SUF_OK;
}
