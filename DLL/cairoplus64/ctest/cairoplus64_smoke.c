/*
 * cairoplus64_smoke.c - hosted functional smoke test for cairoplus64.owd.
 *
 * Links the genuine cairoplus64 module implementation with the host CRT,
 * the k64 API facsimile, and the cairo/pixman vendor archives (never the
 * owrt archive - that shadows the CRT malloc). CAIRO_WIN32_STATIC_BUILD
 * turns cairo_public into plain extern so the cairo64 ABI mirror binds the
 * vendor objects directly.
 *
 * Build & run via cairoplus64_ct.ps1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "cairoplus64.h"
#include "kernel64.h"

k64_status_t k64_initialize_api(void)
{
    return K64_OK;
}

#define W 220
#define H 60

static int failures = 0;

static void check(int cond, const char *what, int line)
{
    if (!cond) {
        fprintf(stderr, "FAIL %s at line %d\n", what, line);
        failures++;
    }
}

static void check_int(long got, long want, const char *what, int line)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got %ld want %ld (line %d)\n",
                what, got, want, line);
        failures++;
    }
}

static uint32_t px(cairo_surface_t *s, int x, int y)
{
    uint8_t *d = cairo_image_surface_get_data(s);
    size_t stride = (size_t)cairo_image_surface_get_stride(s);
    uint32_t v;
    memcpy(&v, d + (size_t)y * stride + (size_t)x * 4u, sizeof v);
    return v;
}

static int window_has_pixel(cairo_surface_t *s, int x0, int y0,
                            int w, int h, uint32_t want)
{
    for (int y = y0; y < y0 + h; y++) {
        for (int x = x0; x < x0 + w; x++) {
            if (px(s, x, y) == want) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    cairoplus64_layer_t layer;
    cairoplus64_status_t st;
    cairo_surface_t *img;
    double adv = -1.0;

    /* --- module identity --- */
    check_int((long)cairoplus64_abi_version(), 10000,
              "abi_version", __LINE__);
    check_int((long)cairoplus64_abi_major(), 1, "abi_major", __LINE__);
    check_int((long)cairoplus64_abi_minor(), 0, "abi_minor", __LINE__);
    check(cairoplus64_ident() != NULL, "ident non-null", __LINE__);

    /* --- palette mapping --- */
    check_int((long)cairoplus64_palette_argb(15), (long)0xFFFFFFFFu,
              "palette 15=white", __LINE__);
    check_int((long)cairoplus64_palette_argb(1), (long)0xFF800000u,
              "palette 1=maroon", __LINE__);
    check_int((long)cairoplus64_palette_argb(0), (long)0xFF000000u,
              "palette 0=black", __LINE__);
    check_int((long)cairoplus64_palette_argb(99), (long)0xFF000000u,
              "palette oob->black", __LINE__);

    /* --- cell width classes (must not depend on module init) --- */
    check_int((long)cairoplus64_cell_width(0x20u), 1, "ASCII w1", __LINE__);
    check_int((long)cairoplus64_cell_width(0xE234u), 1, "PUA w1", __LINE__);
    check_int((long)cairoplus64_cell_width(0x4E00u), 2, "CJK w2", __LINE__);
    check_int((long)cairoplus64_cell_width(0x200Bu), 0, "ZWSP w0", __LINE__);
    check_int((long)cairoplus64_cell_width(0x0301u), 0, "combin w0", __LINE__);
    check_int((long)cairoplus64_cell_width(CAIROPLUS64_GLYPH_BLANK), 0,
              "blank w0", __LINE__);
    check_int((long)cairoplus64_cell_width(CAIROPLUS64_GLYPH_CONT), 0,
              "cont w0", __LINE__);

    /* --- init gate --- */
    st = cairoplus64_module_init();
    check(st == CAIROPLUS64_OK, "module_init", __LINE__);
    st = cairoplus64_layer_open(&layer, NULL, 0xFFFFFFFFu, 0xFF000000u, 20.0);
    check(st == CAIROPLUS64_ERR_BAD_LAYER, "open bad target", __LINE__);

    /* --- surface + layer --- */
    img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    check(cairo_surface_status(img) == CAIRO_STATUS_SUCCESS,
          "image surface", __LINE__);

    {
        cairo_t *cr = cairo_create(img);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_paint(cr);
        cairo_destroy(cr);
        cairo_surface_flush(img);
        check(px(img, 0, 0) == 0xFF000000u, "black canvas", __LINE__);
    }

    st = cairoplus64_layer_open(&layer, img, 0xFFFFFFFFu, 0xFF000000u, 20.0);
    check(st == CAIROPLUS64_OK, "layer_open", __LINE__);

    /* --- UTF-8 stream drawing: toy "HI" + PUA cell + CJK wide --- */
    {
        const uint8_t stream[] = {
            0x48u, 0x49u, 0x20u,                 /* "HI "          */
            0xEEu, 0x88u, 0xB4u,                 /* U+E234 cell     */
            0xE4u, 0xB8u, 0x80u                  /* U+4E00 CJK wide */
        };
        st = cairoplus64_layer_write_sutf8(&layer, 4.0, 40.0,
                                           stream, sizeof stream,
                                           CAIROPLUS64_ATTR_FG_MASK & 15u,
                                           &adv);
        check(st == CAIROPLUS64_OK, "write_sutf8", __LINE__);
        cairoplus64_layer_flush(&layer);

        /* toy glyphs must produce white pixels above baseline (y in
         * [10 (40-asc=16? no: 40-0.8*20=24), 40]) */
        check(window_has_pixel(img, 4, 20, 40, 20, 0xFFFFFFFFu),
              "toy+cell white pixels", __LINE__);
        check(window_has_pixel(img, (int)adv - 20, 24, 20, 16, 0xFFFFFFFFu),
              "wide CJK ink pixels", __LINE__);
    }

    /* --- measure --- */
    {
        const uint8_t one[] = { 0xEEu, 0x88u, 0xB4u };
        const uint8_t cjk[] = { 0xE4u, 0xB8u, 0x80u };
        const uint8_t zw[]  = { 0xE2u, 0x80u, 0x8Bu };   /* U+200B */
        double m;
        st = cairoplus64_layer_measure(&layer, one, sizeof one, &m);
        check(st == CAIROPLUS64_OK && m > 9.9 && m < 10.1,
              "measure PUA=10px", __LINE__);
        st = cairoplus64_layer_measure(&layer, cjk, sizeof cjk, &m);
        check(st == CAIROPLUS64_OK && m > 19.9 && m < 20.1,
              "measure CJK=20px", __LINE__);
        st = cairoplus64_layer_measure(&layer, zw, sizeof zw, &m);
        check(st == CAIROPLUS64_OK && m == 0.0,
              "measure ZW=0", __LINE__);
    }

    /* --- malformed stream must abort cleanly --- */
    {
        const uint8_t bad[] = { 0x48u, 0xFFu, 0x4Du };
        st = cairoplus64_layer_write_sutf8(&layer, 2.0, 40.0,
                                           bad, sizeof bad, 7u, NULL);
        check(st == CAIROPLUS64_ERR_BAD_STREAM, "bad stream rejected",
              __LINE__);
    }

    /* --- layer close / shutdown --- */
    st = cairoplus64_layer_close(&layer);
    check(st == CAIROPLUS64_OK, "layer_close", __LINE__);
    check_int((long)layer.flushes, 2, "flush count", __LINE__);

    cairo_surface_destroy(img);

    st = cairoplus64_module_shutdown();
    check(st == CAIROPLUS64_OK, "module_shutdown", __LINE__);

    if (failures == 0) {
        printf("PASS: cairoplus64 smoke (%d checks)\n", 24);
        return 0;
    }
    fprintf(stderr, "FAIL: %d check(s)\n", failures);
    return 1;
}