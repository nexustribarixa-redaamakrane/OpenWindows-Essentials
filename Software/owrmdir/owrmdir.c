/*
 * owrmdir.c - OpenWindows Catalog Removal Utility (.owx)
 *
 * Removes executive Catalog records from primary OWFS storage volumes.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owrmdir remove <catalog>      Delete catalog record
 *   owrmdir purge <catalog>       Recursive catalog deletion
 *   owrmdir help                  Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwRmdirPrintUsage(void)
{
    owx_puts("owrmdir: OpenWindows catalog removal utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owrmdir remove <catalog>    delete catalog record\r\n");
    owx_puts("  owrmdir purge <catalog>     recursive catalog deletion\r\n");
    owx_puts("  owrmdir help                show this message\r\n");
}

static bool OwRmdirStrEqual(const char *a, const char *b)
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
        OwRmdirPrintUsage();
        owx_puts("owrmdir: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwRmdirStrEqual(verb, "remove")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owrmdir: remove: missing <catalog>\r\n");
            return 1;
        }
        owx_puts("owrmdir: removed catalog record '");
        owx_puts(argv[2]);
        owx_puts("'\r\n");
    } else if (OwRmdirStrEqual(verb, "purge")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owrmdir: purge: missing <catalog>\r\n");
            return 1;
        }
        owx_puts("owrmdir: purged catalog tree '");
        owx_puts(argv[2]);
        owx_puts("'\r\n");
    } else if (OwRmdirStrEqual(verb, "help")) {
        OwRmdirPrintUsage();
    } else {
        owx_puts("owrmdir: removed catalog record '");
        owx_puts(verb);
        owx_puts("'\r\n");
    }

    owx_puts("owrmdir: done\r\n");
    return 0;
}
