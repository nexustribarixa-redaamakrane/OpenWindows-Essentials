/*
 * owtar.c - OpenWindows Archive Utility (.owx)
 *
 * Native executable for the OpenWindows environment.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Constants ────────────────────────────────────────────────── */

#define OWTAR_VERSION "1.0.0"

/* ── Forward declarations ─────────────────────────────────────── */

static int  owtar_run(int argc, const char *const *argv);
static void owtar_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owtar_usage();
        return 1;
    }
    return owtar_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owtar_run(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    /* TODO: implement Archive Utility */
    return 0;
}

static void owtar_usage(void)
{
    /* TODO: print usage for owtar */
    (void)0;
}
