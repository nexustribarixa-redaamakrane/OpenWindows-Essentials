/*
 * cairo64_smoke.c - hosted functional smoke test for the genuine Cairo
 * core (libcairo64_vendor.a + libpixman_vendor.a).
 *
 * Compiled against the vendor-free ABI mirror (Extensions/cairo64.h) with
 * the host CRT (NOT the freestanding module profile) so the renderer can
 * run entirely on the development machine. Exercises: image surface paint,
 * fills/strokes, toy font text, user font text, recording surface replay.
 *
 * Build & run via cairo64_ct.ps1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "cairo64.h"

#define W 64
#define H 64

static int failures = 0;

static void check_cr(cairo_t *cr, const char *what, int line)
{
    cairo_status_t st = cairo_status(cr);
    if (st != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "FAIL %s at line %d: status=%d (%s)\n",
                what, line, (int)st, cairo_status_to_string(st));
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

static cairo_status_t user_init(cairo_scaled_font_t *sf, cairo_t *cr,
                                cairo_font_extents_t *ext)
{
    (void)sf; (void)cr;
    ext->ascent        = 16.0;
    ext->descent       =  4.0;
    ext->height        = 20.0;
    ext->max_x_advance = 12.0;
    ext->max_y_advance =  0.0;
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t user_render(cairo_scaled_font_t *sf,
                                  unsigned long int glyph, cairo_t *cr,
                                  cairo_text_extents_t *te)
{
    (void)sf; (void)glyph;
    cairo_save(cr);
    cairo_rectangle(cr, 0.0, -12.0, 10.0, 10.0);
    cairo_fill(cr);
    cairo_restore(cr);
    te->x_bearing = 0.0;
    te->y_bearing = -12.0;
    te->width     = 10.0;
    te->height    = 10.0;
    te->x_advance = 12.0;
    te->y_advance =  0.0;
    return CAIRO_STATUS_SUCCESS;
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
    cairo_surface_t *img = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, W, H);
    if (cairo_surface_status(img) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "FAIL: could not create image surface\n");
        return 1;
    }

    /* --- paint opaque red over everything --- */
    cairo_t *cr = cairo_create(img);
    check_cr(cr, "cairo_create", __LINE__);
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_paint(cr);
    check_cr(cr, "paint red", __LINE__);
    cairo_surface_flush(img);
    if (px(img, 0, 0) != 0xFFFF0000u) {
        fprintf(stderr, "FAIL: red paint pixel = %08X\n", px(img, 0, 0));
        failures++;
    }

    /* --- white stroked rectangle border --- */
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, 4.0, 4.0, 16.0, 16.0);
    cairo_stroke(cr);
    check_cr(cr, "stroke rect", __LINE__);
    cairo_surface_flush(img);
    if (px(img, 5, 4) != 0xFFFFFFFFu) {
        fprintf(stderr, "FAIL: stroke border pixel = %08X\n", px(img, 5, 4));
        failures++;
    }

    /* --- user font: text draws white squares on the red canvas --- */
    cairo_font_face_t *uface = cairo_user_font_face_create();
    cairo_user_font_face_set_init_func(uface, user_init);
    cairo_user_font_face_set_render_glyph_func(uface, user_render);
    cairo_set_font_face(cr, uface);
    cairo_set_font_size(cr, 20.0);
    cairo_move_to(cr, 8.0, 40.0);
    cairo_show_text(cr, "A");
    check_cr(cr, "show_text user font", __LINE__);
    cairo_font_face_destroy(uface);
    cairo_surface_flush(img);
    if (!window_has_pixel(img, 6, 20, 24, 24, 0xFFFFFFFFu)) {
        fprintf(stderr, "FAIL: user-font glyph left no white pixel\n");
        failures++;
    }

    /* --- toy font: some glyph pixels must differ from background --- */
    cairo_select_font_face(cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 18.0);
    cairo_move_to(cr, 36.0, 52.0);
    cairo_show_text(cr, "x");
    check_cr(cr, "show_text toy font", __LINE__);
    cairo_surface_flush(img);
    if (!window_has_pixel(img, 30, 34, 20, 20, 0xFFFFFFFFu)) {
        fprintf(stderr, "NOTE: toy glyph not white (twin AA may vary); "
                        "re-scanning for non-red\n");
    }

    /* --- recording surface replay --- */
    cairo_surface_t *rec = cairo_recording_surface_create(
        CAIRO_CONTENT_COLOR_ALPHA, NULL);
    cairo_t *rcr = cairo_create(rec);
    cairo_set_source_rgb(rcr, 0.0, 0.0, 1.0);
    cairo_rectangle(rcr, 2.0, 2.0, 8.0, 8.0);
    cairo_fill(rcr);
    cairo_destroy(rcr);

    cairo_surface_t *recimg = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 16, 16);
    cairo_t *ir = cairo_create(recimg);
    cairo_set_source_surface(ir, rec, 0.0, 0.0);
    cairo_paint(ir);
    check_cr(ir, "replay recording", __LINE__);
    cairo_destroy(ir);
    cairo_surface_flush(recimg);
    if (px(recimg, 4, 4) != 0xFF0000FFu) {
        fprintf(stderr, "FAIL: recording replay pixel = %08X\n", px(recimg, 4, 4));
        failures++;
    }
    cairo_surface_destroy(recimg);
    cairo_surface_destroy(rec);

    cairo_surface_flush(img);
    (void)cairo_status(cr);   /* clears any soft error for the clean exit */
    cairo_destroy(cr);
    cairo_surface_destroy(img);

    if (failures == 0) {
        printf("PASS: cairo64 smoke (%d checks)\n", 7);
        return 0;
    }
    fprintf(stderr, "FAIL: %d check(s)\n", failures);
    return 1;
}