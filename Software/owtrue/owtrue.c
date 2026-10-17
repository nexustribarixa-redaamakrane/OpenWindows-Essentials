/*
 * owtrue.c - OpenWindows Always-True Exit Utility (.owx)
 *
 * The identity of the exit-status pair: consumes its argument vector (any
 * number of arguments is legal, exactly as the POSIX utility does), produces no
 * console output, and always reports success.  The useful exit status is the
 * only contract it has.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWTRUE_VERSION "1.0.0"

/* ── Forward declarations ─────────────────────────────────────── */

static int  owtrue_run(int argc, const char *const *argv);
static void owtrue_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owtrue_usage();
        return 0;
    }
    return owtrue_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

/*
 * owtrue_run - report success no matter what was asked.
 *
 * Arguments are intentionally ignored: `owtrue --help`, `owtrue nonsense` and
 * `owtrue` all exit 0, which is what scripts rely on when they use this to
 * reset `$?` or as a no-op branch target.
 */
static int owtrue_run(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    return 0;
}

static void owtrue_usage(void)
{
    owx_puts("owtrue " OWTRUE_VERSION " - always exit 0\r\n");
    owx_puts("usage: owtrue [ignored-args...]\r\n");
}
