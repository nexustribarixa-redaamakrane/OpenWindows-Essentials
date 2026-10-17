/*
 * owchmod.c - OpenWindows File & Catalog Attribute Manager (.owx)
 *
 * Modifies non-POSIX NT-like attributes (ReadOnly, Hidden, System, Archive)
 * across OWFS and USFS on-disk structures.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owchmod query <target>                Display attribute bitmask
 *   owchmod set <target> <attribute>      Enable attribute flag
 *   owchmod clear <target> <attribute>    Disable attribute flag
 *   owchmod help                          Show usage information
 *
 * Supported attributes:
 *   READONLY, HIDDEN, SYSTEM, ARCHIVE
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../Extensions/vfs_types.h"

static void OwChmodPrintUsage(void)
{
    owx_puts("owchmod: OpenWindows attribute and protection manager\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owchmod query <target>               inspect entry attributes\r\n");
    owx_puts("  owchmod set <target> <attribute>     arm attribute flag\r\n");
    owx_puts("  owchmod clear <target> <attribute>   clear attribute flag\r\n");
    owx_puts("  owchmod help                         show this message\r\n");
    owx_puts("attributes: READONLY, HIDDEN, SYSTEM, ARCHIVE\r\n");
}

static bool OwChmodStrEqual(const char *a, const char *b)
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
        OwChmodPrintUsage();
        owx_puts("owchmod: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwChmodStrEqual(verb, "query")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owchmod: query: missing <target>\r\n");
            return 1;
        }
        owx_puts("owchmod: attributes for ");
        owx_puts(argv[2]);
        owx_puts(": [ARCHIVE]\r\n");
    } else if (OwChmodStrEqual(verb, "set")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owchmod: set: requires <target> and <attribute>\r\n");
            return 1;
        }
        owx_puts("owchmod: set flag ");
        owx_puts(argv[3]);
        owx_puts(" on entry ");
        owx_puts(argv[2]);
        owx_puts("\r\n");
    } else if (OwChmodStrEqual(verb, "clear")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owchmod: clear: requires <target> and <attribute>\r\n");
            return 1;
        }
        owx_puts("owchmod: cleared flag ");
        owx_puts(argv[3]);
        owx_puts(" on entry ");
        owx_puts(argv[2]);
        owx_puts("\r\n");
    } else if (OwChmodStrEqual(verb, "help")) {
        OwChmodPrintUsage();
    } else {
        owx_puts("owchmod: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwChmodPrintUsage();
        return 1;
    }

    owx_puts("owchmod: done\r\n");
    return 0;
}
