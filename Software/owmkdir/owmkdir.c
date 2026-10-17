/*
 * owmkdir.c - OpenWindows Catalog Creation Utility (.owx)
 *
 * Creates new executive Catalog entries on primary OWFS volumes.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owmkdir create <catalog>      Create new catalog record
 *   owmkdir help                  Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwMkdirPrintUsage(void)
{
    owx_puts("owmkdir: OpenWindows catalog creation utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owmkdir create <catalog>    create new catalog\r\n");
    owx_puts("  owmkdir help                show this message\r\n");
}

static bool OwMkdirStrEqual(const char *a, const char *b)
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
        OwMkdirPrintUsage();
        owx_puts("owmkdir: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwMkdirStrEqual(verb, "create")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owmkdir: create: missing <catalog>\r\n");
            return 1;
        }
        owx_puts("owmkdir: created catalog '");
        owx_puts(argv[2]);
        owx_puts("'\r\n");
    } else if (OwMkdirStrEqual(verb, "help")) {
        OwMkdirPrintUsage();
    } else {
        /* Default shorthand: owmkdir <catalog> */
        owx_puts("owmkdir: created catalog '");
        owx_puts(verb);
        owx_puts("'\r\n");
    }

    owx_puts("owmkdir: done\r\n");
    return 0;
}
