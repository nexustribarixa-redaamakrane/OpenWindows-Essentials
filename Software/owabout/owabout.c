/*
 * owabout.c - OpenWindows About Dialog (.owx)
 *
 * Prints the OpenWindows about banner: product name, release version, build
 * number, target architecture, subsystem list and credits.  `-v` adds the
 * detailed build record (repository set, compiler contract, kernel ABI).
 *
 * This is a formatting program: every line is composed from the constants below
 * through the Ring 3 console gateway in owx_gate.h, with no file and no
 * environment read, so the banner is identical wherever it is printed.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Build record ─────────────────────────────────────────────── */

#define OWABOUT_VERSION "1.0.0"
#define OWABOUT_BUILD   "20260117.4"
#define OWABOUT_REVISION "b41c7e2"
#define OWABOUT_ARCH    "x86_64"
#define OWABOUT_CPL     "Ring 3 (CPL3)"

/* ── Banner fragments ─────────────────────────────────────────── */

static const char *const s_banner[] = {
    "  +------------------------------------------------------------+",
    "  |                                                            |",
    "  |   OpenWindows  -  a clean-slate, non-POSIX, NT-like OS     |",
    "  |                                                            |",
    "  +------------------------------------------------------------+"
};

static const char *const s_subsystems[] = {
    "kernel core       clean-slate NT-like kernel, scheduler and HAL",
    "ring3 gateway     owx_gate.h: console, process and OWFS calls",
    "storage           OWFS and USFS volumes, CRC32c and Fletcher",
    "drivers (.owc)    bare-metal drivers and kernel modules",
    "runtime (.owd)    freestanding dynamic libraries",
    "sentinel          .sentinel damage-control recovery directives",
    "superunicode      SUCS, SUTF transports and .suf fonts",
    "bancode           fault diagnostics and kernel trap table",
    "vip               UniVIP and FVIP volume indexing"
};

static const char *const s_credits[] = {
    "OpenWindows project contributors",
    "reactos and the wider NT-clone community, for prior art",
    "the freestanding C99 toolchain that builds every subsystem"
};

#define OWABOUT_BANNER_N (sizeof(s_banner) / sizeof(s_banner[0]))
#define OWABOUT_SUBSYSTEM_N (sizeof(s_subsystems) / sizeof(s_subsystems[0]))
#define OWABOUT_CREDITS_N (sizeof(s_credits) / sizeof(s_credits[0]))

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owabout_streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static void owabout_line(const char *s)
{
    owx_puts(s);
    owx_puts("\r\n");
}

/* ── Entry point ──────────────────────────────────────────────── */

static int  owabout_run(int argc, const char *const *argv);
static void owabout_usage(void);

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owabout_usage();
        return 1;
    }
    return owabout_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owabout_run(int argc, const char *const *argv)
{
    bool verbose = false;
    int i;

    for (i = 1; i < argc && argv != NULL && argv[i] != NULL; i++) {
        const char *a = argv[i];
        if (owabout_streq(a, "-v") || owabout_streq(a, "--verbose")) {
            verbose = true;
        } else if (owabout_streq(a, "-h") || owabout_streq(a, "--help")) {
            owabout_usage();
            return 0;
        } else {
            owx_puts("owabout: unknown option: ");
            owx_puts(a);
            owx_puts("\r\n");
            owabout_usage();
            return 1;
        }
    }

    for (uint32_t i = 0u; i < OWABOUT_BANNER_N; i++) {
        owabout_line(s_banner[i]);
    }

    owx_puts("  version ");
    owx_puts(OWABOUT_VERSION);
    owx_puts("  build ");
    owx_puts(OWABOUT_BUILD);
    owx_puts("  ");
    owx_puts(OWABOUT_ARCH);
    owx_puts("\r\n");
    owx_puts("  pid ");
    owx_put_i64(owx_getpid());
    owx_puts("  ");
    owx_puts(OWABOUT_CPL);
    owx_puts("\r\n\r\n");

    owabout_line("  Subsystems:");
    for (uint32_t i = 0u; i < OWABOUT_SUBSYSTEM_N; i++) {
        owx_puts("    ");
        owabout_line(s_subsystems[i]);
    }

    if (verbose) {
        owx_puts("\r\n");
        owabout_line("  Build record:");
        owx_puts("    compiler   freestanding C99, -nostdlib -Wall -Wextra -Werror -Wpedantic\r\n");
        owx_puts("    revision   ");
        owx_puts(OWABOUT_REVISION);
        owx_puts("\r\n");
        owx_puts("    gate       int $0x80 -> OwSyscallDispatch()\r\n");
        owx_puts("    repos      OpenWindows, OpenWindows-Essentials, Modular-Bootloader\r\n");
        owx_puts("               superunicode, BANcode, OpenWindows-Storage, vip\r\n");
    }

    owx_puts("\r\n");
    owabout_line("  Credits:");
    for (uint32_t i = 0u; i < OWABOUT_CREDITS_N; i++) {
        owx_puts("    ");
        owabout_line(s_credits[i]);
    }

    owx_puts("\r\n  owabout ");
    owx_puts(OWABOUT_VERSION);
    owx_puts(" (");
    owx_puts(OWABOUT_BUILD);
    owx_puts(") - OpenWindows Project, MIT-style open license\r\n");
    return 0;
}

static void owabout_usage(void)
{
    owx_puts("owabout " OWABOUT_VERSION " - OpenWindows about banner\r\n");
    owx_puts("usage: owabout        print the product banner\r\n");
    owx_puts("       owabout -v     include the build record\r\n");
}
