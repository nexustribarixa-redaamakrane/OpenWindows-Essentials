/*
 * owpaint.c - OpenWindows Simple Paint Application (.owx)
 *
 * Native executable for the OpenWindows environment.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Constants ────────────────────────────────────────────────── */

#define OWPAINT_VERSION "1.0.0"

/* ── Forward declarations ─────────────────────────────────────── */

static int  owpaint_run(int argc, const char *const *argv);
static void owpaint_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owpaint_usage();
        return 1;
    }
    return owpaint_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owpaint_run(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    /* TODO: implement Simple Paint Application */
    return 0;
}

static void owpaint_usage(void)
{
    /* TODO: print usage for owpaint */
    (void)0;
}
