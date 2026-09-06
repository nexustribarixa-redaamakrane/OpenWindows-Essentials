/**
 * @file suf_inspector.c
 * @brief SuperUnicode Font (.suf) File Inspector & Validator CLI
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "suf/suf_parser.h"

static void dump_boot_bitmap_ascii(const uint8_t *bmp, uint8_t width, uint8_t height, uint8_t bpp) {
    if (bpp == 1) {
        uint32_t row_bytes = (width + 7) / 8;
        for (uint32_t r = 0; r < height; ++r) {
            printf("      |");
            const uint8_t *row = bmp + (r * row_bytes);
            for (uint32_t c = 0; c < width; ++c) {
                bool bit = (row[c / 8] & (0x80 >> (c % 8))) != 0;
                printf(bit ? "██" : "  ");
            }
            printf("|\n");
        }
    } else if (bpp == 8) {
        for (uint32_t r = 0; r < height; ++r) {
            printf("      |");
            const uint8_t *row = bmp + (r * width);
            for (uint32_t c = 0; c < width; ++c) {
                uint8_t a = row[c];
                if (a > 192) printf("██");
                else if (a > 128) printf("▓▓");
                else if (a > 64) printf("▒▒");
                else if (a > 0) printf("░░");
                else printf("  ");
            }
            printf("|\n");
        }
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <font.suf> [--preview-glyph <gid>]\n", argv[0]);
        return 1;
    }

    const char *filepath = argv[1];
    FILE *f = fopen(filepath, "rb");
    if (!f) {
        fprintf(stderr, "Error: Failed to open file '%s'\n", filepath);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fprintf(stderr, "Error: Empty file '%s'\n", filepath);
        fclose(f);
        return 1;
    }

    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        fclose(f);
        return 1;
    }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        fprintf(stderr, "Error: Failed to read file content\n");
        free(buf);
        fclose(f);
        return 1;
    }
    fclose(f);

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(buf, (size_t)sz, &hdr);
    if (st != SUF_OK) {
        fprintf(stderr, "Error: .suf validation failed with status %d\n", st);
        free(buf);
        return 1;
    }

    printf("=================================================================\n");
    printf(" SuperUnicode Font (.suf) Header Inspector\n");
    printf(" File: %s (%ld bytes)\n", filepath, sz);
    printf("=================================================================\n");
    printf(" Magic:              0x%08X ('%c%c%c%c')\n",
           (unsigned int)hdr.magic,
           (char)(hdr.magic & 0xFF),
           (char)((hdr.magic >> 8) & 0xFF),
           (char)((hdr.magic >> 16) & 0xFF),
           (char)((hdr.magic >> 24) & 0xFF));
    printf(" Version:            %u\n", hdr.version);
    printf(" Flags:              0x%04X (", hdr.flags);
    if (hdr.flags & SUF_FLAG_BOOT_BITMAP) printf("BOOT_BITMAP ");
    if (hdr.flags & SUF_FLAG_OS_VECTOR) printf("OS_VECTOR ");
    if (hdr.flags & SUF_FLAG_EXTSUCS) printf("EXTSUCS(64-bit) ");
    if (hdr.flags & SUF_FLAG_LIGATURES) printf("LIGATURES ");
    if (hdr.flags & SUF_FLAG_KERNING) printf("KERNING ");
    if (hdr.flags & SUF_FLAG_BANCODE) printf("BANCODE ");
    if (hdr.flags & SUF_FLAG_VARIABLE) printf("VARIABLE ");
    if (hdr.flags & SUF_FLAG_PLUGIN_FONT) printf("PLUGIN_FONT ");
    printf(")\n");
    printf(" Glyph Count:        %u\n", hdr.glyph_count);
    printf(" Units Per EM:       %u\n", hdr.units_per_em);
    printf(" Typo Metrics:       Ascender: %d, Descender: %d, Line Gap: %d\n",
           hdr.ascender, hdr.descender, hdr.line_gap);
    printf(" Bounding Box:       [%d, %d] -> [%d, %d]\n",
           hdr.bbox_min_x, hdr.bbox_min_y, hdr.bbox_max_x, hdr.bbox_max_y);
    printf(" Boot Bitmap Spec:   %ux%u @ %u bpp\n",
           hdr.boot_bitmap_width, hdr.boot_bitmap_height, hdr.boot_bitmap_bpp);
    printf(" Checksum (CRC32):   0x%08X\n", (unsigned int)hdr.checksum);
    printf("-----------------------------------------------------------------\n");
    printf(" Table Offsets & Layout:\n");
    printf("   cmap:             offset 0x%04X, size %u bytes\n", hdr.cmap_offset, hdr.cmap_size);
    printf("   metrics (16B):    offset 0x%04X, size %u bytes (align16: %s)\n",
           hdr.metrics_offset, hdr.metrics_size, (hdr.metrics_offset % 16 == 0) ? "YES" : "NO");
    printf("   boot_bitmap:      offset 0x%04X, size %u bytes\n", hdr.boot_bitmap_offset, hdr.boot_bitmap_size);
    printf("   outlines:         offset 0x%04X, size %u bytes\n", hdr.outlines_offset, hdr.outlines_size);
    printf("   kerning:          offset 0x%04X, size %u bytes (%u pairs)\n",
           hdr.kerning_offset, hdr.kerning_size, (unsigned int)(hdr.kerning_size / sizeof(suf_kern_pair_t)));
    printf("   ligatures:        offset 0x%04X, size %u bytes (%u rules)\n",
           hdr.ligatures_offset, hdr.ligatures_size, (unsigned int)(hdr.ligatures_size / sizeof(suf_ligature_t)));
    printf("   var_axes:         offset 0x%04X, size %u bytes (%u axes)\n",
           hdr.var_axes_offset, hdr.var_axes_size, (unsigned int)(hdr.var_axes_size / sizeof(suf_var_axis_t)));
    printf("   plugin_meta:      offset 0x%04X, size %u bytes\n",
           hdr.plugin_meta_offset, hdr.plugin_meta_size);
    printf("=================================================================\n");

    /* Variable Axes Display */
    uint32_t axis_count = 0;
    if (suf_get_axis_count(buf, (size_t)sz, &axis_count) == SUF_OK && axis_count > 0) {
        printf(" Continuous Variable Design Axes (%u Active):\n", axis_count);
        for (uint32_t i = 0; i < axis_count; ++i) {
            suf_var_axis_t ax;
            if (suf_get_axis_info(buf, (size_t)sz, i, &ax) == SUF_OK) {
                printf("   [%u] Tag: '%c%c%c%c' (0x%08X), Range: [%.1f .. %.1f] (Default: %.1f), Name: \"%s\"\n",
                       i,
                       (char)((ax.tag >> 24) & 0xFF),
                       (char)((ax.tag >> 16) & 0xFF),
                       (char)((ax.tag >> 8) & 0xFF),
                       (char)(ax.tag & 0xFF),
                       (unsigned int)ax.tag,
                       ax.min_val, ax.max_val, ax.def_val, ax.name);
            }
        }
        printf("-----------------------------------------------------------------\n");
    }

    /* Plugin Metadata Display */
    if (hdr.flags & SUF_FLAG_PLUGIN_FONT) {
        suf_plugin_font_meta_t pm;
        if (suf_get_plugin_meta(buf, (size_t)sz, &pm) == SUF_OK) {
            printf(" Modded SuperUnicode Plugin Metadata (Extended Mode):\n");
            printf("   Plugin ID:        %s\n", pm.plugin_id);
            printf("   Version:          %u.%u.%u\n", pm.ver_major, pm.ver_minor, pm.ver_patch);
            printf("   Partition FS:     %s\n", pm.fs_type == 1 ? "OWFS (OpenWindows File System)" : "Unknown");
            printf("   Range Count:      %u\n", pm.range_count);
            printf("   Base Ceiling:     0x%llX\n", (unsigned long long)pm.base_limit);
            printf("-----------------------------------------------------------------\n");
        }
    }

    /* Glyph preview */
    uint32_t preview_gid = 0;
    if (argc >= 4 && strcmp(argv[2], "--preview-glyph") == 0) {
        preview_gid = (uint32_t)atoi(argv[3]);
    }

    if (preview_gid < hdr.glyph_count && (hdr.flags & SUF_FLAG_BOOT_BITMAP)) {
        suf_metric_t m;
        suf_get_glyph_metric(buf, (size_t)sz, preview_gid, &m);
        const uint8_t *bmp = NULL;
        size_t bsz = 0;
        suf_get_boot_bitmap(buf, (size_t)sz, preview_gid, &bmp, &bsz);

        printf(" Preview Glyph ID #%u:\n", preview_gid);
        printf("   Advance Width: %d, LSB: %d, BBox: [%d,%d]..[%d,%d]\n",
               m.advance_width, m.left_side_bearing, m.x_min, m.y_min, m.x_max, m.y_max);
        if (bmp) {
            dump_boot_bitmap_ascii(bmp, hdr.boot_bitmap_width, hdr.boot_bitmap_height, hdr.boot_bitmap_bpp);
        }
    }

    free(buf);
    return 0;
}
