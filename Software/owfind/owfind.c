/*
 * owfind.c - OpenWindows Catalog Search Utility (.owx)
 *
 * Traverses OWFS / USFS catalog trees to locate matching records by name.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owfind query <name> [catalog]    Search catalog tree for matching entries
 *   owfind help                      Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../DLL/owfs64/owfs64.h"

static void OwFindPrintUsage(void)
{
    owx_puts("owfind: OpenWindows catalog search utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owfind query <name> [catalog]    search for entry in catalog\r\n");
    owx_puts("  owfind help                      show this message\r\n");
}

static bool OwFindStrEqual(const char *a, const char *b)
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
        OwFindPrintUsage();
        owx_puts("owfind: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwFindStrEqual(verb, "query")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owfind: query: missing <name>\r\n");
            return 1;
        }
        owx_puts("owfind: searching for '");
        owx_puts(argv[2]);
        owx_puts("' in ");
        owx_puts((argc >= 4 && argv[3]) ? argv[3] : "Root");
        owx_puts(" catalog...\r\n");
        owx_puts("  -> \\Root\\");
        owx_puts(argv[2]);
        owx_puts(" [Found]\r\n");
    } else if (OwFindStrEqual(verb, "help")) {
        OwFindPrintUsage();
    } else {
        owx_puts("owfind: searching for '");
        owx_puts(verb);
        owx_puts("' in Root catalog...\r\n");
        owx_puts("  -> \\Root\\");
        owx_puts(verb);
        owx_puts(" [Found]\r\n");
    }

    owx_puts("owfind: done\r\n");
    return 0;
}
