/*
 * owfalse.c - OpenWindows Always-False Exit Utility (.owx)
 *
 * The mirror of owtrue: accepts any argument vector, writes nothing, and always
 * reports failure.  A script uses it as a deliberately failing branch target or
 * to seed a nonzero status at the start of a guarded sequence.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWFALSE_VERSION "1.0.0"

/* ── Forward declarations ─────────────────────────────────────── */

static int  owfalse_run(int argc, const char *const *argv);
static void owfalse_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owfalse_usage();
        return 1;
    }
    return owfalse_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

/*
 * owfalse_run - report failure no matter what was asked.
 *
 * The exit status is the whole point: it is a fixed 1, not an error code that
 * depends on the arguments.  Nothing is printed so that a caller can use it
 * inside command substitution without polluting the captured text.
 */
static int owfalse_run(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    return 1;
}

static void owfalse_usage(void)
{
    owx_puts("owfalse " OWFALSE_VERSION " - always exit 1\r\n");
    owx_puts("usage: owfalse [ignored-args...]\r\n");
}
