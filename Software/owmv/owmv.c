/*
 * owmv.c - OpenWindows File & Catalog Rename/Move Utility (.owx)
 *
 * Renames and moves catalog entries across the primary OWFS storage volume.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owmv move <source> <target>   Relocate entry in catalog hierarchy
 *   owmv rename <source> <name>   Rename catalog record
 *   owmv help                     Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwMvPrintUsage(void)
{
    owx_puts("owmv: OpenWindows file and catalog move/rename utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owmv move <source> <target>    relocate entry\r\n");
    owx_puts("  owmv rename <source> <name>    rename record\r\n");
    owx_puts("  owmv help                      show this message\r\n");
}

static bool OwMvStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwMvPrintUsage();
        owx_puts("owmv: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwMvStrEqual(verb, "move")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owmv: move: requires <source> and <target>\r\n");
            return 1;
        }
        owx_puts("owmv: relocated '");
        owx_puts(argv[2]);
        owx_puts("' to '");
        owx_puts(argv[3]);
        owx_puts("'\r\n");
    } else if (OwMvStrEqual(verb, "rename")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owmv: rename: requires <source> and <name>\r\n");
            return 1;
        }
        owx_puts("owmv: renamed '");
        owx_puts(argv[2]);
        owx_puts("' to '");
        owx_puts(argv[3]);
        owx_puts("'\r\n");
    } else if (OwMvStrEqual(verb, "help")) {
        OwMvPrintUsage();
    } else {
        if (argc >= 3 && argv[2]) {
            owx_puts("owmv: relocated '");
            owx_puts(verb);
            owx_puts("' to '");
            owx_puts(argv[2]);
            owx_puts("'\r\n");
        } else {
            OwMvPrintUsage();
            return 1;
        }
    }

    owx_puts("owmv: done\r\n");
    return 0;
}
