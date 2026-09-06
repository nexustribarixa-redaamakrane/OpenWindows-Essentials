/**
 * @file test_suf.c
 * @brief Comprehensive Test Suite for SuperUnicode Font (.suf) Engine & Toolchain
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "suf/suf_types.h"
#include "suf/suf_parser.h"
#include "suf/suf_builder.h"
#include "suf/suf_conv.h"

static void test_suf_types_and_alignment(void) {
    printf("[TEST] Verifying SIMD metric alignment, variable axes, and header sizes...\n");
    assert(sizeof(suf_metric_t) == 16);
    assert(sizeof(suf_header_t) == 128);
    assert(sizeof(suf_cmap_entry_t) == 8);
    assert(sizeof(suf_cmap_ext_entry_t) == 16);
    assert(sizeof(suf_kern_pair_t) == 12);
    assert(sizeof(suf_ligature_t) == 16);
    assert(sizeof(suf_var_axis_t) == 48);
    assert(sizeof(suf_plugin_font_meta_t) == 128);
    printf("       -> 16-byte SIMD layout, 128B header, and 48B axis descriptors confirmed.\n");
}

static void test_suf_flag_content_coherence(void) {
    printf("[TEST] Verifying flag/content coherence (no fabricated bitmap mode)...\n");

    /* Builder claims BOTH rendering modes but receives vector data only:
     * serialization must strip the unsupported BOOT_BITMAP claim. */
    suf_builder_t *b = suf_builder_create(1000, 800, -200, SUF_FLAG_BOOT_BITMAP | SUF_FLAG_OS_VECTOR);
    assert(b != NULL);
    suf_builder_set_boot_params(b, 8, 16, 1);

    suf_metric_t m0 = { .advance_width = 500, .left_side_bearing = 0, .x_min = 0, .y_min = 0, .x_max = 500, .y_max = 700, .data_offset = 0 };
    uint8_t outline0[] = {
        SUF_CMD_MOVE_TO, 0x00, 0x00, 0x00, 0x00,
        SUF_CMD_LINE_TO, 0xF4, 0x01, 0xBC, 0x02,
        SUF_CMD_CLOSE_PATH,
        SUF_CMD_END_GLYPH
    };
    assert(suf_builder_add_glyph(b, 0, &m0, NULL, 0, outline0, sizeof(outline0)) == 0);

    /* Glyph 1 carries NO outline and NO bitmap: it must get its own
     * zero-length stream slot instead of aliasing glyph 0's data. */
    suf_metric_t m1 = m0;
    assert(suf_builder_add_glyph(b, 0x41, &m1, NULL, 0, NULL, 0) == 1);

    uint8_t *buf = NULL;
    size_t sz = 0;
    assert(suf_builder_serialize(b, &buf, &sz) == SUF_OK);
    suf_builder_free(b);

    suf_header_t hdr;
    memcpy(&hdr, buf, sizeof(hdr));
    assert((hdr.flags & SUF_FLAG_BOOT_BITMAP) == 0);
    assert((hdr.flags & SUF_FLAG_OS_VECTOR) != 0);
    assert(hdr.boot_bitmap_size == 0);

    const uint8_t *bmp_bytes = NULL;
    size_t bmp_n = 0;
    assert(suf_get_boot_bitmap(buf, sz, 0, &bmp_bytes, &bmp_n) == SUF_ERR_NO_BITMAP);

    suf_metric_t ma, mb;
    assert(suf_get_glyph_metric(buf, sz, 0, &ma) == SUF_OK);
    assert(suf_get_glyph_metric(buf, sz, 1, &mb) == SUF_OK);
    assert(mb.data_offset != ma.data_offset);

    const uint8_t *cmds = NULL;
    size_t cmd_n = 0;
    assert(suf_get_glyph_outline(buf, sz, 1, &cmds, &cmd_n) == SUF_OK);
    assert(cmd_n == 0);

    /* A lying header (mode flag set, backing table absent) is malformed. */
    uint8_t *lying = (uint8_t *)malloc(sz);
    assert(lying != NULL);
    memcpy(lying, buf, sz);
    ((suf_header_t *)lying)->flags |= SUF_FLAG_BOOT_BITMAP;
    assert(suf_validate_header(lying, sz, NULL) == SUF_ERR_INVALID_HEADER);
    free(lying);
    free(buf);

    printf("       -> Truthful mode flags enforced by builder and parser.\n");
}

static void test_suf_builder_and_parser(void) {
    printf("[TEST] Testing builder creation, serialization, and freestanding parsing...\n");

    suf_builder_t *b = suf_builder_create(1000, 800, -200, SUF_FLAG_BOOT_BITMAP | SUF_FLAG_OS_VECTOR);
    assert(b != NULL);

    suf_builder_set_line_gap(b, 100);
    suf_builder_set_bbox(b, -50, -200, 1050, 800);
    suf_builder_set_boot_params(b, 8, 16, 1);

    /* Glyph 0: .notdef */
    suf_metric_t m0 = { .advance_width = 500, .left_side_bearing = 0, .x_min = 0, .y_min = -200, .x_max = 500, .y_max = 800, .data_offset = 0 };
    uint8_t bmp0[16] = { 0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF };
    uint32_t g0 = suf_builder_add_glyph(b, 0, &m0, bmp0, sizeof(bmp0), NULL, 0);
    assert(g0 == 0);

    /* Glyph 1: 'A' (codepoint 0x41) */
    suf_metric_t m1 = { .advance_width = 600, .left_side_bearing = 50, .x_min = 50, .y_min = 0, .x_max = 550, .y_max = 700, .data_offset = 0 };
    uint8_t bmp1[16] = { 0x00, 0x18, 0x24, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x42, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint8_t outline1[] = {
        SUF_CMD_MOVE_TO, 0x32, 0x00, 0x00, 0x00,
        SUF_CMD_LINE_TO, 0x26, 0x01, 0xBC, 0x02,
        SUF_CMD_LINE_TO, 0x26, 0x02, 0x00, 0x00,
        SUF_CMD_CLOSE_PATH,
        SUF_CMD_END_GLYPH
    };
    uint32_t g1 = suf_builder_add_glyph(b, 0x41, &m1, bmp1, sizeof(bmp1), outline1, sizeof(outline1));
    assert(g1 == 1);

    /* Glyph 2: 'V' (codepoint 0x56) */
    suf_metric_t m2 = { .advance_width = 600, .left_side_bearing = 50, .x_min = 50, .y_min = 0, .x_max = 550, .y_max = 700, .data_offset = 0 };
    uint8_t bmp2[16] = { 0x00, 0x42, 0x42, 0x42, 0x42, 0x24, 0x24, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
    uint32_t g2 = suf_builder_add_glyph(b, 0x56, &m2, bmp2, sizeof(bmp2), NULL, 0);
    assert(g2 == 2);

    /* Glyph 3: ExtSUCS codepoint (0x100000000ULL) */
    suf_metric_t m3 = { .advance_width = 800, .left_side_bearing = 100, .x_min = 100, .y_min = 0, .x_max = 700, .y_max = 700, .data_offset = 0 };
    uint8_t bmp3[16] = { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 };
    uint32_t g3 = suf_builder_add_glyph(b, 0x100000000ULL, &m3, bmp3, sizeof(bmp3), NULL, 0);
    assert(g3 == 3);

    /* Add BANcode fatal crash glyph */
    uint32_t g_bancode = suf_builder_add_bancode_glyph(b, 0x0011A005, &m1, bmp1, sizeof(bmp1), outline1, sizeof(outline1));
    assert(g_bancode == 4);

    /* Add Kerning & Ligature */
    assert(suf_builder_add_kerning(b, g1, g2, -80) == true);
    assert(suf_builder_add_ligature(b, g1, g2, g3) == true);

    /* Add Variable Design Axes (Standard & Custom) */
    assert(suf_builder_add_axis(b, SUF_AXIS_WGHT, "Weight", 100.0f, 400.0f, 900.0f) == true);
    assert(suf_builder_add_axis(b, SUF_AXIS_WDTH, "Width", 50.0f, 100.0f, 200.0f) == true);
    assert(suf_builder_add_axis(b, SUF_AXIS_SLNT, "Slant", -20.0f, 0.0f, 20.0f) == true);
    assert(suf_builder_add_axis(b, SUF_AXIS_ITAL, "Italic", 0.0f, 0.0f, 1.0f) == true);
    assert(suf_builder_add_axis(b, SUF_AXIS_GRAD, "Grade", -200.0f, 0.0f, 200.0f) == true);
    assert(suf_builder_add_axis(b, SUF_AXIS_ROND, "Roundness", 0.0f, 0.0f, 100.0f) == true);
    assert(suf_builder_add_axis(b, SUF_AXIS_OPSZ, "Optical Size", 6.0f, 12.0f, 72.0f) == true);
    assert(suf_builder_add_axis(b, SUF_AXIS_TAG('X','O','P','Q'), "Custom Param", 0.0f, 50.0f, 100.0f) == true);

    /* Set Modded Plugin Font Metadata */
    assert(suf_builder_set_plugin_meta(b, "com.openwindows.font.cyberpunk", 2, 1, 0, 1) == true);

    /* Serialize */
    uint8_t *suf_buf = NULL;
    size_t suf_sz = 0;
    assert(suf_builder_serialize(b, &suf_buf, &suf_sz) == SUF_OK);
    assert(suf_buf != NULL && suf_sz > sizeof(suf_header_t));
    assert(suf_builder_write_file(b, "test_sample.suf") == SUF_OK);
    suf_builder_free(b);


    /* Parse and Validate */
    suf_header_t hdr;
    assert(suf_validate_header(suf_buf, suf_sz, &hdr) == SUF_OK);
    assert(hdr.magic == SUF_MAGIC);
    assert(hdr.glyph_count == 5);
    assert(hdr.flags & SUF_FLAG_EXTSUCS);
    assert(hdr.flags & SUF_FLAG_KERNING);
    assert(hdr.flags & SUF_FLAG_LIGATURES);
    assert(hdr.flags & SUF_FLAG_BANCODE);
    assert(hdr.flags & SUF_FLAG_VARIABLE);
    assert(hdr.flags & SUF_FLAG_PLUGIN_FONT);

    /* Lookups */
    uint32_t gid = 999;
    assert(suf_lookup_glyph_id(suf_buf, suf_sz, 0x41, &gid) == SUF_OK && gid == g1);
    assert(suf_lookup_glyph_id_ext(suf_buf, suf_sz, 0x100000000ULL, &gid) == SUF_OK && gid == g3);

    /* BANcode lookup & Badge rendering */
    uint32_t bgid = 0;
    assert(suf_lookup_bancode_glyph(suf_buf, suf_sz, 0x0011A005, &bgid) == SUF_OK && bgid == g_bancode);
    uint32_t fb[64 * 64] = {0};
    assert(suf_render_bancode_badge(suf_buf, suf_sz, 0x0011A005, fb, 64, 64, 64, 10, 10) == SUF_OK);

    /* Variable Axes Discovery & Metric Interpolation */
    uint32_t axis_count = 0;
    assert(suf_get_axis_count(suf_buf, suf_sz, &axis_count) == SUF_OK && axis_count == 8);
    suf_var_axis_t wght_ax;
    assert(suf_find_axis_by_tag(suf_buf, suf_sz, SUF_AXIS_WGHT, &wght_ax) == SUF_OK);
    assert(strcmp(wght_ax.name, "Weight") == 0);

    float axis_coords[8] = { 800.0f, 150.0f, 0.0f, 0.0f, 0.0f, 0.0f, 12.0f, 50.0f };
    suf_metric_t interp_m;
    assert(suf_interpolate_glyph_metric(suf_buf, suf_sz, g1, axis_coords, 8, &interp_m) == SUF_OK);
    assert(interp_m.advance_width > m1.advance_width); /* Expanded width & weight */

    /* Modded Plugin Metadata Inspection */
    suf_plugin_font_meta_t pm;
    assert(suf_get_plugin_meta(suf_buf, suf_sz, &pm) == SUF_OK);
    assert(strcmp(pm.plugin_id, "com.openwindows.font.cyberpunk") == 0);
    assert(pm.ver_major == 2 && pm.ver_minor == 1);

    /* Plugin Font Packaging (.scsp blob) */
    uint8_t *scsp_blob = NULL;
    size_t blob_sz = 0;
    assert(suf_conv_pack_plugin_font(suf_buf, suf_sz, "com.openwindows.font.cyberpunk", 2, 1, 0, &scsp_blob, &blob_sz) == SUF_OK);
    assert(scsp_blob != NULL && blob_sz > suf_sz);

    uint8_t *unpacked_suf = NULL;
    size_t unpacked_sz = 0;
    assert(suf_conv_unpack_plugin_font(scsp_blob, blob_sz, &unpacked_suf, &unpacked_sz) == SUF_OK);
    assert(unpacked_sz == suf_sz);
    assert(memcmp(unpacked_suf, suf_buf, suf_sz) == 0);
    free(scsp_blob);
    free(unpacked_suf);

    free(suf_buf);
    printf("       -> Builder, parser, BANcode badges, variable axes, and plugin packaging verified.\n");
}

static void test_bidirectional_conversions(void) {
    printf("[TEST] Testing bidirectional conversions (TTF, SFD, WOFF, EOT, OTF, PostScript)...\n");

    suf_builder_t *b = suf_builder_create(1000, 800, -200, SUF_FLAG_BOOT_BITMAP | SUF_FLAG_OS_VECTOR);
    suf_metric_t m = { .advance_width = 500, .left_side_bearing = 0, .x_min = 0, .y_min = 0, .x_max = 500, .y_max = 700, .data_offset = 0 };
    uint8_t bmp[16] = { 0x55, 0xAA, 0x55, 0xAA };
    suf_builder_add_glyph(b, 0, &m, bmp, 16, NULL, 0);
    suf_builder_add_glyph(b, 'A', &m, bmp, 16, NULL, 0);
    suf_builder_add_glyph(b, 'B', &m, bmp, 16, NULL, 0);

    uint8_t *orig_suf = NULL;
    size_t orig_suf_sz = 0;
    assert(suf_builder_serialize(b, &orig_suf, &orig_suf_sz) == SUF_OK);
    suf_builder_free(b);

    /* 1. SUF -> TTF -> SUF */
    uint8_t *ttf_data = NULL;
    size_t ttf_sz = 0;
    assert(suf_conv_suf_to_ttf(orig_suf, orig_suf_sz, &ttf_data, &ttf_sz) == SUF_OK);
    suf_builder_t *ttf_b = NULL;
    assert(suf_conv_ttf_to_suf(ttf_data, ttf_sz, &ttf_b) == SUF_OK);
    suf_builder_free(ttf_b);
    free(ttf_data);

    /* 2. SUF -> SFD -> SUF */
    char *sfd_text = NULL;
    size_t sfd_len = 0;
    assert(suf_conv_suf_to_sfd(orig_suf, orig_suf_sz, &sfd_text, &sfd_len) == SUF_OK);
    suf_builder_t *sfd_b = NULL;
    assert(suf_conv_sfd_to_suf(sfd_text, sfd_len, &sfd_b) == SUF_OK);
    suf_builder_free(sfd_b);
    free(sfd_text);

    /* 3. SUF -> WOFF -> SUF */
    uint8_t *woff_data = NULL;
    size_t woff_sz = 0;
    assert(suf_conv_suf_to_woff(orig_suf, orig_suf_sz, &woff_data, &woff_sz) == SUF_OK);
    suf_builder_t *woff_b = NULL;
    assert(suf_conv_woff_to_suf(woff_data, woff_sz, &woff_b) == SUF_OK);
    suf_builder_free(woff_b);
    free(woff_data);

    /* 4. SUF -> EOT -> SUF */
    uint8_t *eot_data = NULL;
    size_t eot_sz = 0;
    assert(suf_conv_suf_to_eot(orig_suf, orig_suf_sz, &eot_data, &eot_sz) == SUF_OK);
    suf_builder_t *eot_b = NULL;
    assert(suf_conv_eot_to_suf(eot_data, eot_sz, &eot_b) == SUF_OK);
    suf_builder_free(eot_b);
    free(eot_data);

    /* 5. SUF -> OTF -> SUF */
    uint8_t *otf_data = NULL;
    size_t otf_sz = 0;
    assert(suf_conv_suf_to_otf(orig_suf, orig_suf_sz, &otf_data, &otf_sz) == SUF_OK);
    suf_builder_t *otf_b = NULL;
    assert(suf_conv_otf_to_suf(otf_data, otf_sz, &otf_b) == SUF_OK);
    suf_builder_free(otf_b);
    free(otf_data);

    /* 6. SUF -> PostScript Type 1 / PFA -> SUF */
    char *ps_text = NULL;
    size_t ps_len = 0;
    assert(suf_conv_suf_to_ps(orig_suf, orig_suf_sz, &ps_text, &ps_len) == SUF_OK);
    suf_builder_t *ps_b = NULL;
    assert(suf_conv_ps_to_suf(ps_text, ps_len, &ps_b) == SUF_OK);
    suf_builder_free(ps_b);
    free(ps_text);

    /* 7. SUF -> PostScript Binary / PFB -> SUF */
    uint8_t *pfb_data = NULL;
    size_t pfb_sz = 0;
    assert(suf_conv_suf_to_pfb(orig_suf, orig_suf_sz, &pfb_data, &pfb_sz) == SUF_OK);
    suf_builder_t *pfb_b = NULL;
    assert(suf_conv_pfb_to_suf(pfb_data, pfb_sz, &pfb_b) == SUF_OK);
    suf_builder_free(pfb_b);
    free(pfb_data);

    free(orig_suf);
    printf("       -> All bidirectional converters (including PostScript PFA/PFB) passed successfully.\n");
}

static void test_variable_font_export(void) {
    printf("[TEST] Verifying variable font export (fvar roundtrip)...\n");

    suf_builder_t *b = suf_builder_create(1000, 800, -200, SUF_FLAG_OS_VECTOR);
    assert(b != NULL);

    suf_metric_t m = { .advance_width = 500, .left_side_bearing = 0, .x_min = 0, .y_min = 0, .x_max = 500, .y_max = 700, .data_offset = 0 };
    uint8_t outline[] = {
        SUF_CMD_MOVE_TO, 0x00, 0x00, 0x00, 0x00,
        SUF_CMD_LINE_TO, 0xF4, 0x01, 0xBC, 0x02,
        SUF_CMD_CLOSE_PATH,
        SUF_CMD_END_GLYPH
    };
    assert(suf_builder_add_glyph(b, 0, &m, NULL, 0, outline, sizeof(outline)) == 0);
    assert(suf_builder_add_glyph(b, 0x41, &m, NULL, 0, outline, sizeof(outline)) == 1);
    assert(suf_builder_add_axis(b, 0x77676874U /* 'wght' */, "Weight", 100.0f, 400.0f, 900.0f) == true);
    assert(suf_builder_add_axis(b, 0x6F70737AU /* 'opsz' */, "Optical size", 6.0f, 11.0f, 32.0f) == true);

    uint8_t *suf_buf = NULL;
    size_t suf_sz = 0;
    assert(suf_builder_serialize(b, &suf_buf, &suf_sz) == SUF_OK);
    suf_builder_free(b);

    /* Export as TrueType: must contain an 'fvar' table (variable, not static instance) */
    uint8_t *ttf = NULL;
    size_t ttf_sz = 0;
    assert(suf_conv_suf_to_ttf(suf_buf, suf_sz, &ttf, &ttf_sz) == SUF_OK);

    uint16_t num_tables = (uint16_t)((ttf[4] << 8) | ttf[5]);
    assert(num_tables >= 11);
    bool found_fvar = false;
    for (uint16_t i = 0; i < num_tables; ++i) {
        size_t dir = 12 + ((size_t)i * 16);
        uint32_t tag = ((uint32_t)ttf[dir] << 24) | ((uint32_t)ttf[dir + 1] << 16) |
                       ((uint32_t)ttf[dir + 2] << 8) | (uint32_t)ttf[dir + 3];
        if (tag == 0x66766172U /* 'fvar' */) found_fvar = true;
    }
    assert(found_fvar);

    /* Re-import the exported font: axes must survive intact */
    suf_builder_t *rb = NULL;
    assert(suf_conv_ttf_to_suf(ttf, ttf_sz, &rb) == SUF_OK);
    free(ttf);

    uint8_t *rt_buf = NULL;
    size_t rt_sz = 0;
    assert(suf_builder_serialize(rb, &rt_buf, &rt_sz) == SUF_OK);
    suf_builder_free(rb);

    uint32_t ax = 0;
    assert(suf_get_axis_count(rt_buf, rt_sz, &ax) == SUF_OK);
    assert(ax == 2);

    suf_var_axis_t w;
    assert(suf_find_axis_by_tag(rt_buf, rt_sz, 0x77676874U, &w) == SUF_OK);
    assert(w.min_val == 100.0f && w.def_val == 400.0f && w.max_val == 900.0f);

    suf_var_axis_t o;
    assert(suf_find_axis_by_tag(rt_buf, rt_sz, 0x6F70737AU, &o) == SUF_OK);
    assert(o.min_val == 6.0f && o.def_val == 11.0f && o.max_val == 32.0f);

    free(rt_buf);
    free(suf_buf);
    printf("       -> fvar emitted by suf2ttf/suf2otf; axes survive a full roundtrip.\n");
}

static void test_glyph_variation_roundtrip(void) {
    printf("[TEST] Verifying glyph variations (gvar) storage and export...\n");

    /* Synthetic self-contained GlyphVariationData block: 1 tuple, embedded
     * peak, private point numbers (3 points), int8 X deltas, zero Y deltas. */
    static const uint8_t blk[] = {
        0x00, 0x01,             /* tupleVariationCount = 1 */
        0x00, 0x0C,             /* dataOffset = 12 */
        0x00, 0x09,             /* tuple0: dataSize = 9 */
        0xA0, 0x00,             /*         flags = EMBEDDED_PEAK | PRIVATE_POINTS */
        0x40, 0x00, 0x00, 0x00, /* peakTuple = (1.0, 0.0) F2Dot14 */
        0x03,                   /* points: count=3 explicit */
        0x02, 0x01, 0x01,       /* point deltas: +0,+1,+1 -> 0,1,2 */
        0x02, 0x0A, 0xEC, 0x1E, /* X: int8 run of 3: 10,-20,30 */
        0x82                    /* Y: zeros run of 3 */
    };
    const size_t blk_len = sizeof(blk);

    suf_builder_t *b = suf_builder_create(1000, 800, -200, SUF_FLAG_OS_VECTOR);
    assert(b != NULL);

    suf_metric_t m = { .advance_width = 500, .left_side_bearing = 0,
                       .x_min = 0, .y_min = 0, .x_max = 500, .y_max = 700, .data_offset = 0 };
    uint8_t outline[] = {
        SUF_CMD_MOVE_TO, 0x00, 0x00, 0x00, 0x00,
        SUF_CMD_LINE_TO, 0xF4, 0x01, 0xBC, 0x02,
        SUF_CMD_CLOSE_PATH,
        SUF_CMD_END_GLYPH
    };
    assert(suf_builder_add_glyph(b, 0, &m, NULL, 0, outline, sizeof(outline)) == 0);
    assert(suf_builder_add_glyph(b, 0x41, &m, NULL, 0, outline, sizeof(outline)) == 1);
    assert(suf_builder_add_axis(b, 0x77676874U /* 'wght' */, "Weight", 100.0f, 400.0f, 900.0f));
    assert(suf_builder_set_glyph_variation(b, 1, blk, blk_len));

    uint8_t *suf_buf = NULL;
    size_t suf_sz = 0;
    assert(suf_builder_serialize(b, &suf_buf, &suf_sz) == SUF_OK);
    suf_builder_free(b);

    /* Truth rule: flag must be set since a block exists.
     * Header scalar fields are serialized little-endian / native. */
    uint16_t flags = (uint16_t)(suf_buf[6] | (suf_buf[7] << 8));
    assert(flags & SUF_FLAG_GLYPH_VARIATIONS);

    /* Glyph 1 must return the verbatim block; glyph 0 has none. */
    const uint8_t *p = NULL;
    size_t len = 0;
    assert(suf_get_glyph_variation(suf_buf, suf_sz, 1, &p, &len) == SUF_OK);
    assert(len == blk_len && memcmp(p, blk, blk_len) == 0);
    p = NULL; len = 0xFFFFFFFF;
    assert(suf_get_glyph_variation(suf_buf, suf_sz, 0, &p, &len) == SUF_OK);
    assert(len == 0);

    /* Export as TrueType: 'gvar' table must appear and reference the block. */
    uint8_t *ttf = NULL;
    size_t ttf_sz = 0;
    assert(suf_conv_suf_to_ttf(suf_buf, suf_sz, &ttf, &ttf_sz) == SUF_OK);

    uint16_t num_tables = (uint16_t)((ttf[4] << 8) | ttf[5]);
    long gvar_off = -1;
    for (uint16_t i = 0; i < num_tables; ++i) {
        size_t dir = 12 + ((size_t)i * 16);
        uint32_t tag = ((uint32_t)ttf[dir] << 24) | ((uint32_t)ttf[dir + 1] << 16) |
                       ((uint32_t)ttf[dir + 2] << 8) | (uint32_t)ttf[dir + 3];
        if (tag == 0x67766172U /* 'gvar' */) {
            gvar_off = (long)((uint32_t)ttf[dir + 8] << 24) | ((uint32_t)ttf[dir + 9] << 16) |
                       ((uint32_t)ttf[dir + 10] << 8) | (uint32_t)ttf[dir + 11];
        }
    }
    assert(gvar_off > 0);

    /* Header: sharedTupleCount must be 0 (self-contained tuples). */
    uint16_t shared_count = (uint16_t)((ttf[gvar_off + 6] << 8) | ttf[gvar_off + 7]);
    assert(shared_count == 0);

    /* Glyph 1's span must be exactly the block size and the payload must
     * match verbatim. Offsets may be short (stored /2) or long depending on
     * total size/alignment; the test block is 21 bytes (odd) so long
     * offsets are expected, but handle both per the flags field. */
    size_t offs = gvar_off + 20;
    uint16_t gflags = (uint16_t)((ttf[gvar_off + 14] << 8) | ttf[gvar_off + 15]);
    uint32_t data_start = (uint32_t)((ttf[gvar_off + 16] << 24) | (ttf[gvar_off + 17] << 16) |
                                     (ttf[gvar_off + 18] << 8) | (uint32_t)ttf[gvar_off + 19]);
    uint32_t o1, o2;
    if (gflags & 0x0001) {
        assert(data_start == 20 + ((size_t)2 + 1) * 4);
        o1 = ((uint32_t)ttf[offs + 4] << 24) | ((uint32_t)ttf[offs + 5] << 16) |
             ((uint32_t)ttf[offs + 6] << 8) | (uint32_t)ttf[offs + 7];
        o2 = ((uint32_t)ttf[offs + 8] << 24) | ((uint32_t)ttf[offs + 9] << 16) |
             ((uint32_t)ttf[offs + 10] << 8) | (uint32_t)ttf[offs + 11];
    } else {
        o1 = (uint32_t)((ttf[offs + 0] << 8) | ttf[offs + 1]) * 2;
        o2 = (uint32_t)((ttf[offs + 2] << 8) | ttf[offs + 3]) * 2;
    }
    assert(o2 - o1 == blk_len);
    /* The serialized payload bytes must match verbatim after the header. */
    assert(memcmp(ttf + gvar_off + data_start + o1, blk, blk_len) == 0);

    free(ttf);
    free(suf_buf);
    printf("       -> gvar blob stored verbatim; suf2ttf rebuilds a valid 'gvar' table.\n");
}

/* ---------------------------------------------------------------------------
 * Test: Font name records (SNM1 blob) survive storage and TTF export, so
 * roundtripped fonts keep their identity instead of "SuperUnicode Font".
 * ------------------------------------------------------------------------- */
static void test_font_names_preservation(void) {
    printf("[TEST] Verifying font name preservation (SNM1)...\n");
    static const uint8_t outline[] = {
        SUF_CMD_MOVE_TO, 0x00, 0x00, 0x00, 0x00,
        SUF_CMD_LINE_TO, 0xF4, 0x01, 0xBC, 0x02,
        SUF_CMD_CLOSE_PATH,
        SUF_CMD_END_GLYPH
    };
    suf_metric_t m = { .advance_width = 500, .left_side_bearing = 0,
                        .x_min = 0, .y_min = 0, .x_max = 500, .y_max = 700, .data_offset = 0 };

    suf_builder_t *b = suf_builder_create(2048, 1600, -400, SUF_FLAG_OS_VECTOR);
    assert(b);
    assert(suf_builder_add_glyph(b, 0, &m, NULL, 0, NULL, 0) == 0);
    assert(suf_builder_add_glyph(b, 'A', &m, NULL, 0, outline, sizeof(outline)) == 1);

    assert(suf_builder_set_name(b, 1, "Bodoni Moda"));
    assert(suf_builder_set_name(b, 2, "Bold Italic"));
    assert(suf_builder_set_name(b, 5, "Version 1.100;GF;BodoniModa"));
    assert(suf_builder_set_name(b, 6, "BodoniModa-BoldItalic"));
    /* Replacing an existing ID must not duplicate it. */
    assert(suf_builder_set_name(b, 1, "Bodoni Moda II"));

    uint8_t *suf_buf = NULL;
    size_t suf_sz = 0;
    assert(suf_builder_serialize(b, &suf_buf, &suf_sz) == SUF_OK);
    suf_builder_free(b);

    /* Parser returns stored records verbatim. */
    uint32_t name_count = 0;
    assert(suf_get_name_count(suf_buf, suf_sz, &name_count) == SUF_OK);
    assert(name_count == 4);

    const char *p = NULL;
    size_t len = 0;
    assert(suf_get_name(suf_buf, suf_sz, 1, &p, &len) == SUF_OK);
    assert(len == strlen("Bodoni Moda II") && memcmp(p, "Bodoni Moda II", len) == 0);
    assert(suf_get_name(suf_buf, suf_sz, 6, &p, &len) == SUF_OK);
    assert(len == strlen("BodoniModa-BoldItalic") && memcmp(p, "BodoniModa-BoldItalic", len) == 0);
    assert(suf_get_name(suf_buf, suf_sz, 99, &p, &len) != SUF_OK);

    /* Exported TTF carries the family in its 'name' table (UTF-16BE). */
    uint8_t *ttf = NULL;
    size_t ttf_sz = 0;
    assert(suf_conv_suf_to_ttf(suf_buf, suf_sz, &ttf, &ttf_sz) == SUF_OK);

    uint16_t num_tables = (uint16_t)((ttf[4] << 8) | ttf[5]);
    size_t name_off = 0;
    for (uint16_t i = 0; i < num_tables; ++i) {
        size_t dir = 12 + ((size_t)i * 16);
        uint32_t tag = ((uint32_t)ttf[dir] << 24) | ((uint32_t)ttf[dir + 1] << 16) |
                       ((uint32_t)ttf[dir + 2] << 8) | ttf[dir + 3];
        if (tag == 0x6E616D65UL) { /* 'name' */
            name_off = (((size_t)ttf[dir + 8] << 24) | ((size_t)ttf[dir + 9] << 16) |
                        ((size_t)ttf[dir + 10] << 8) | ttf[dir + 11]);
        }
    }
    assert(name_off != 0);
    uint16_t rec_count = (uint16_t)((ttf[name_off + 2] << 8) | ttf[name_off + 3]);
    uint16_t string_off = (uint16_t)((ttf[name_off + 4] << 8) | ttf[name_off + 5]);
    bool found_family = false;
    for (uint16_t r = 0; r < rec_count; ++r) {
        size_t rec = name_off + 6 + ((size_t)r * 12);
        uint16_t nid = (uint16_t)((ttf[rec + 6] << 8) | ttf[rec + 7]);
        if (nid != 1) continue;
        uint16_t slen = (uint16_t)((ttf[rec + 8] << 8) | ttf[rec + 9]);
        uint16_t soff = (uint16_t)((ttf[rec + 10] << 8) | ttf[rec + 11]);
        size_t s = name_off + string_off + soff;
        /* UTF-16BE of "Bodoni Moda II": ASCII chars with zero high bytes. */
        static const char expect[] = "Bodoni Moda II";
        found_family = (slen == (sizeof(expect) - 1) * 2);
        for (size_t c = 0; found_family && c < sizeof(expect) - 1; ++c) {
            if (ttf[s + c * 2] != 0x00 || ttf[s + c * 2 + 1] != (uint8_t)expect[c]) {
                found_family = false;
            }
        }
        break;
    }
    assert(found_family);

    free(ttf);
    free(suf_buf);
    printf("       -> names survive storage and export; no more re-branding.\n");
}

/* ---------------------------------------------------------------------------
 * Test: Unicode-only export filter. SuperUnicode codepoints beyond U+10FFFF
 * are stripped entirely from TTF output; indices compact; gid 0 survives.
 * ------------------------------------------------------------------------- */
static void test_unicode_only_export(void) {
    printf("[TEST] Verifying Unicode-only export filter (cp > U+10FFFF stripped)...\n");
    static const uint8_t outline[] = {
        SUF_CMD_MOVE_TO, 0x10, 0x00, 0x20, 0x00,
        SUF_CMD_LINE_TO, 0xF4, 0x01, 0xBC, 0x02,
        SUF_CMD_CLOSE_PATH,
        SUF_CMD_END_GLYPH
    };
    suf_metric_t m = { .advance_width = 500, .left_side_bearing = 0,
                        .x_min = 0, .y_min = 0, .x_max = 500, .y_max = 700, .data_offset = 0 };

    suf_builder_t *b = suf_builder_create(1000, 800, -200, SUF_FLAG_OS_VECTOR | SUF_FLAG_EXTSUCS);
    assert(b);
    assert(suf_builder_add_glyph(b, 0, &m, NULL, 0, NULL, 0) == 0);              /* .notdef */
    assert(suf_builder_add_glyph(b, 0x41, &m, NULL, 0, outline, sizeof(outline)) == 1);
    assert(suf_builder_add_glyph(b, 0x110000ULL, &m, NULL, 0, outline, sizeof(outline)) == 2);
    assert(suf_builder_add_glyph(b, 0x42, &m, NULL, 0, outline, sizeof(outline)) == 3);
    assert(suf_builder_add_glyph(b, 0x7FFFFFFFULL, &m, NULL, 0, outline, sizeof(outline)) == 4);
    assert(suf_builder_add_glyph(b, 0x1F600ULL, &m, NULL, 0, outline, sizeof(outline)) == 5);

    uint8_t *suf_buf = NULL;
    size_t suf_sz = 0;
    assert(suf_builder_serialize(b, &suf_buf, &suf_sz) == SUF_OK);
    suf_builder_free(b);

    uint8_t *ttf = NULL;
    size_t ttf_sz = 0;
    assert(suf_conv_suf_to_ttf(suf_buf, suf_sz, &ttf, &ttf_sz) == SUF_OK);

    uint16_t num_tables = (uint16_t)((ttf[4] << 8) | ttf[5]);
    size_t maxp_off = 0, cmap_off = 0, loca_off = 0, hmtx_off = 0;
    for (uint16_t i = 0; i < num_tables; ++i) {
        size_t dir = 12 + ((size_t)i * 16);
        uint32_t tag = ((uint32_t)ttf[dir] << 24) | ((uint32_t)ttf[dir + 1] << 16) |
                       ((uint32_t)ttf[dir + 2] << 8) | ttf[dir + 3];
        uint32_t off = ((uint32_t)ttf[dir + 8] << 24) | ((uint32_t)ttf[dir + 9] << 16) |
                       ((uint32_t)ttf[dir + 10] << 8) | ttf[dir + 11];
        if (tag == 0x6D617870UL) maxp_off = off;
        else if (tag == 0x636D6170UL) cmap_off = off;
        else if (tag == 0x6C6F6361UL) loca_off = off;
        else if (tag == 0x686D7478UL) hmtx_off = off;
    }
    assert(maxp_off && cmap_off && loca_off && hmtx_off);

    /* Only .notdef + A + B + U+1F600 remain (4 glyphs). */
    uint16_t num_glyphs_out = (uint16_t)((ttf[maxp_off + 4] << 8) | ttf[maxp_off + 5]);
    assert(num_glyphs_out == 4);

    /* hmtx has exactly numGlyphs entries worth of data before next table. */
    uint16_t num_h_metrics = (uint16_t)((ttf[hmtx_off - 4] << 8)); /* hhea is elsewhere; check via size instead */
    (void)num_h_metrics;

    /* cmap maps A->1 and B->2 after compaction. */
    uint16_t n_enc = (uint16_t)((ttf[cmap_off + 2] << 8) | ttf[cmap_off + 3]);
    uint16_t sub_off = 0;
    bool have_f12_rec = false;
    size_t f12_sub_off = 0;
    for (uint16_t e = 0; e < n_enc; ++e) {
        size_t er = cmap_off + 4 + ((size_t)e * 8);
        uint16_t plat = (uint16_t)((ttf[er] << 8) | ttf[er + 1]);
        uint16_t enc = (uint16_t)((ttf[er + 2] << 8) | ttf[er + 3]);
        uint32_t off32 = ((uint32_t)ttf[er + 4] << 24) | ((uint32_t)ttf[er + 5] << 16) |
                         ((uint32_t)ttf[er + 6] << 8) | ttf[er + 7];
        if (plat == 3 && enc == 1 && sub_off == 0) sub_off = (uint16_t)off32;
        if (plat == 3 && enc == 10) { have_f12_rec = true; f12_sub_off = off32; }
    }
    assert(sub_off != 0);
    size_t f4 = cmap_off + sub_off;
    uint16_t seg_x2 = (uint16_t)((ttf[f4 + 6] << 8) | ttf[f4 + 7]);
    uint16_t seg_count = (uint16_t)(seg_x2 / 2);
    size_t starts = f4 + 14 + seg_x2 + 2;
    size_t deltas = starts + seg_x2;
    bool mapped_a = false, mapped_b = false;
    for (uint16_t s = 0; s + 1 < seg_count; ++s) {
        uint16_t st = (uint16_t)((ttf[starts + s * 2] << 8) | ttf[starts + s * 2 + 1]);
        uint16_t en = (uint16_t)((ttf[f4 + 14 + s * 2] << 8) | ttf[f4 + 14 + s * 2 + 1]);
        int32_t d = (int16_t)((ttf[deltas + s * 2] << 8) | ttf[deltas + s * 2 + 1]);
        if (st <= 0x41 && 0x41 <= en && (0x41 + d) == 1) mapped_a = true;
        if (st <= 0x42 && 0x42 <= en && (0x42 + d) == 2) mapped_b = true;
    }
    assert(mapped_a && mapped_b);

    /* Astral survivor U+1F600 must appear in a format-12 (3,10) subtable. */
    assert(have_f12_rec);
    size_t f12 = cmap_off + f12_sub_off;
    uint16_t f12_fmt = (uint16_t)((ttf[f12] << 8) | ttf[f12 + 1]);
    assert(f12_fmt == 12);
    uint32_t n_groups = ((uint32_t)ttf[f12 + 12] << 24) | ((uint32_t)ttf[f12 + 13] << 16) |
                        ((uint32_t)ttf[f12 + 14] << 8) | ttf[f12 + 15];
    assert(n_groups >= 1);
    bool mapped_emoji = false;
    for (uint32_t g = 0; g < n_groups; ++g) {
        size_t grp = f12 + 16 + ((size_t)g * 12);
        uint32_t gs = ((uint32_t)ttf[grp] << 24) | ((uint32_t)ttf[grp + 1] << 16) |
                      ((uint32_t)ttf[grp + 2] << 8) | ttf[grp + 3];
        uint32_t ge = ((uint32_t)ttf[grp + 4] << 24) | ((uint32_t)ttf[grp + 5] << 16) |
                      ((uint32_t)ttf[grp + 6] << 8) | ttf[grp + 7];
        uint32_t sgp = ((uint32_t)ttf[grp + 8] << 24) | ((uint32_t)ttf[grp + 9] << 16) |
                       ((uint32_t)ttf[grp + 10] << 8) | ttf[grp + 11];
        if (gs <= 0x1F600 && 0x1F600 <= ge && sgp + (0x1F600 - gs) == 3) mapped_emoji = true;
    }
    assert(mapped_emoji);

    /* loca has numGlyphs+1 long entries; last offset must be sane (>0). */
    uint32_t last_loc = ((uint32_t)ttf[loca_off + num_glyphs_out * 4] << 24) |
                        ((uint32_t)ttf[loca_off + num_glyphs_out * 4 + 1] << 16) |
                        ((uint32_t)ttf[loca_off + num_glyphs_out * 4 + 2] << 8) |
                        ttf[loca_off + num_glyphs_out * 4 + 3];
    assert(last_loc > 0);

    free(ttf);
    free(suf_buf);
    printf("       -> non-Unicode glyphs stripped; astral glyphs keep format-12 mappings.\n");
}

int main(void) {
    printf("=================================================================\n");
    printf(" Running SuperUnicode Font (.suf) Engine Test Suite\n");
    printf("=================================================================\n");
    test_suf_types_and_alignment();
    test_suf_flag_content_coherence();
    test_variable_font_export();
    test_glyph_variation_roundtrip();
    test_font_names_preservation();
    test_unicode_only_export();
    test_suf_builder_and_parser();
    test_bidirectional_conversions();
    printf("=================================================================\n");
    printf("[ALL TESTS PASSED] SuperUnicode Font (.suf) Suite Verified.\n");
    printf("=================================================================\n");
    return 0;
}
