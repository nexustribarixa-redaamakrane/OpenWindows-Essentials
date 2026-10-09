/*
 * owimgview.c - OpenWindows Image Viewer (.owx)
 *
 * Native executable for the OpenWindows environment.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Constants ────────────────────────────────────────────────── */

#define OWIMGVIEW_VERSION "1.0.0"

/* ── Forward declarations ─────────────────────────────────────── */

static int  owimgview_run(int argc, const char *const *argv);
static void owimgview_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owimgview_usage();
        return 1;
    }
    return owimgview_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owimgview_run(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    /* TODO: implement Image Viewer */
    return 0;
}

static void owimgview_usage(void)
{
    /* TODO: print usage for owimgview */
    (void)0;
}
