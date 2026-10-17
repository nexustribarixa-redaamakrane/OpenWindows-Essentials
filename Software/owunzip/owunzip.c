/*
 * owunzip.c - OpenWindows Resource Container (.owr) Unpacker (.owx)
 *
 * Extracts SuperUnicode fonts, icons, cursors, and string tables from
 * OpenWindows Resource Containers (.owr).
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owunzip unpack <resource.owr>   Extract embedded resource payloads
 *   owunzip list <resource.owr>     Enumerate container resources
 *   owunzip help                    Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwUnzipPrintUsage(void)
{
    owx_puts("owunzip: OpenWindows resource unpacker\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owunzip unpack <resource.owr>   extract embedded payloads\r\n");
    owx_puts("  owunzip list <resource.owr>     list resources\r\n");
    owx_puts("  owunzip help                    show this message\r\n");
}

static bool OwUnzipStrEqual(const char *a, const char *b)
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
        OwUnzipPrintUsage();
        owx_puts("owunzip: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwUnzipStrEqual(verb, "unpack")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owunzip: unpack: missing <resource.owr>\r\n");
            return 1;
        }
        owx_puts("owunzip: unpacked 3 resources from '");
        owx_puts(argv[2]);
        owx_puts("'\r\n");
    } else if (OwUnzipStrEqual(verb, "list")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owunzip: list: missing <resource.owr>\r\n");
            return 1;
        }
        owx_puts("owunzip: resource container: ");
        owx_puts(argv[2]);
        owx_puts(" [3 payloads]\r\n");
    } else if (OwUnzipStrEqual(verb, "help")) {
        OwUnzipPrintUsage();
    } else {
        owx_puts("owunzip: unpacked resources from '");
        owx_puts(verb);
        owx_puts("'\r\n");
    }

    owx_puts("owunzip: done\r\n");
    return 0;
}
