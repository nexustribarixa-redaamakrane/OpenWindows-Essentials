/*
 * userinterface64_smoke.c - hosted functional smoke test for
 * userinterface64.owd.
 *
 * Links the genuine userinterface64 module implementation with the host
 * CRT, the k64 API facsimile, and the cairo/pixman vendor archives (never
 * the owrt archive). CAIRO_WIN32_STATIC_BUILD turns cairo_public into
 * plain extern so the cairo64 ABI mirror binds the vendor objects.
 *
 * Build & run via userinterface64_ct.ps1.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "userinterface64.h"
#include "kernel64.h"

k64_status_t k64_initialize_api(void)
{
    return K64_OK;
}

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

int main(void)
{
    userinterface64_window_t *win;
    userinterface64_status_t st;

    /* --- identity --- */
    check_int((long)userinterface64_abi_version(), 10000,
              "abi_version", __LINE__);
    check_int((long)userinterface64_abi_major(), 1, "abi_major", __LINE__);
    check_int((long)userinterface64_abi_minor(), 0, "abi_minor", __LINE__);
    check(userinterface64_ident() != NULL, "ident non-null", __LINE__);

    /* --- gatekeeping before init --- */
    win = userinterface64_window_create(8u, 8u, 0xFFFFFFFFu);
    check(win == NULL, "create before init -> NULL", __LINE__);

    st = userinterface64_module_init();
    check(st == UI64_OK, "module_init", __LINE__);
    st = userinterface64_module_init();
    check(st == UI64_OK, "module_init idempotent", __LINE__);

    /* --- window create / geometry / bg fill --- */
    win = userinterface64_window_create(0u, 8u, 0xFFFFFFFFu);
    check(win == NULL, "zero width -> NULL", __LINE__);

    win = userinterface64_window_create(32u, 24u, 0xFF008000u);
    check(win != NULL, "window_create", __LINE__);
    check_int((long)userinterface64_window_width(win), 32,
              "width", __LINE__);
    check_int((long)userinterface64_window_height(win), 24,
              "height", __LINE__);

    {
        cairo_surface_t *s = userinterface64_window_surface(win);
        check(s != NULL, "window_surface", __LINE__);
        check_int((long)px(s, 0, 0), (long)0xFF008000u,
                  "bg pixel green", __LINE__);
        check_int((long)px(s, 31, 23), (long)0xFF008000u,
                  "bg pixel far corner", __LINE__);
    }

    /* --- clean repaint --- */
    st = userinterface64_window_clear(win, 0xFF0000FFu);
    check(st == UI64_OK, "window_clear", __LINE__);
    check_int((long)px(userinterface64_window_surface(win), 16, 12),
              (long)0xFF0000FFu, "cleared blue", __LINE__);

    /* --- client cairo drawing into the buffer --- */
    {
        cairo_surface_t *s = userinterface64_window_surface(win);
        cairo_t *cr = cairo_create(s);
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_rectangle(cr, 10.0, 10.0, 6.0, 6.0);
        cairo_fill(cr);
        cairo_destroy(cr);
        cairo_surface_flush(s);
        check_int((long)px(s, 12, 12), (long)0xFFFF0000u,
                  "client-drawn red", __LINE__);
    }

    /* --- present onto a red target --- */
    {
        cairo_surface_t *target = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, 64, 48);
        cairo_t *cr = cairo_create(target);
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_paint(cr);
        cairo_destroy(cr);
        cairo_surface_flush(target);

        st = userinterface64_window_present(win, target, 0, 0);
        check(st == UI64_OK, "present full in-bounds", __LINE__);
        check_int((long)px(target, 4, 4), (long)0xFF0000FFu,
                  "window bg blue@target", __LINE__);
        check_int((long)px(target, 14, 14), (long)0xFFFF0000u,
                  "client red reflected", __LINE__);

        st = userinterface64_window_present(win, target, 60, 60);
        check(st == UI64_ERR_CLIPPED, "present off-target clipped",
              __LINE__);
        check_int((long)px(target, 40, 30), (long)0xFFFF0000u,
                  "target untouched under clipped win", __LINE__);

        cairo_surface_destroy(target);
    }

    /* --- bad args --- */
    st = userinterface64_window_present(NULL, NULL, 0, 0);
    check(st == UI64_ERR_BAD_WINDOW, "present NULL win", __LINE__);
    st = userinterface64_window_present(win, NULL, 0, 0);
    check(st == UI64_ERR_BAD_TARGET, "present NULL target", __LINE__);
    st = userinterface64_window_clear(NULL, 0xFF000000u);
    check(st == UI64_ERR_BAD_WINDOW, "clear NULL win", __LINE__);

    /* --- teardown --- */
    userinterface64_window_destroy(win);
    userinterface64_window_destroy(NULL);      /* no-op, must not crash */

    /* --- window count limits --- */
    {
        int i;
        userinterface64_window_t *ws[48];
        int n = 0;
        for (i = 0; i < 48; i++) {
            ws[i] = userinterface64_window_create(4u, 4u, 0xFF000000u);
            if (ws[i]) {
                n++;
            }
        }
        check_int(n, 32, "arena caps at 32 windows", __LINE__);
        for (i = 0; i < n; i++) {
            userinterface64_window_destroy(ws[i]);
        }
        ws[0] = userinterface64_window_create(4u, 4u, 0xFF000000u);
        check(ws[0] != NULL, "arena reuses slots", __LINE__);
        userinterface64_window_destroy(ws[0]);
    }

    st = userinterface64_module_shutdown();
    check(st == UI64_OK, "module_shutdown", __LINE__);
    win = userinterface64_window_create(4u, 4u, 0xFFFFFFFFu);
    check(win == NULL, "create after shutdown -> NULL", __LINE__);
    st = userinterface64_module_shutdown();
    check(st == UI64_ERR_UNINITIALIZED, "double shutdown", __LINE__);

    if (failures == 0) {
        printf("PASS: userinterface64 smoke (%d checks)\n", 27);
        return 0;
    }
    fprintf(stderr, "FAIL: %d check(s)\n", failures);
    return 1;
}