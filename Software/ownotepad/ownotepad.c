/*
 * ownotepad.c - OpenWindows Simple Notepad (.owx)
 *
 * Native executable for the OpenWindows environment.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Constants ────────────────────────────────────────────────── */

#define OWNOTEPAD_VERSION "1.0.0"

/* ── Forward declarations ─────────────────────────────────────── */

static int  ownotepad_run(int argc, const char *const *argv);
static void ownotepad_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        ownotepad_usage();
        return 1;
    }
    return ownotepad_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int ownotepad_run(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    /* TODO: implement Simple Notepad */
    return 0;
}

static void ownotepad_usage(void)
{
    /* TODO: print usage for ownotepad */
    (void)0;
}
