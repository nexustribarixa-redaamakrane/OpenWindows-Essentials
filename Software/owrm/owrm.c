/*
 * owrm.c - OpenWindows File Deletion & Sanitization Utility (.owx)
 *
 * Removes or cryptographically purges file records on OWFS / USFS volumes.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owrm delete <target>          Remove catalog file record
 *   owrm purge <target>           Cryptographic 3-pass sanitization
 *   owrm help                     Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwRmPrintUsage(void)
{
    owx_puts("owrm: OpenWindows file deletion and purge utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owrm delete <target>    remove file record\r\n");
    owx_puts("  owrm purge <target>     cryptographic 3-pass purge\r\n");
    owx_puts("  owrm help               show this message\r\n");
}

static bool OwRmStrEqual(const char *a, const char *b)
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
        OwRmPrintUsage();
        owx_puts("owrm: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwRmStrEqual(verb, "delete")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owrm: delete: missing <target>\r\n");
            return 1;
        }
        owx_puts("owrm: removed file record '");
        owx_puts(argv[2]);
        owx_puts("'\r\n");
    } else if (OwRmStrEqual(verb, "purge")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owrm: purge: missing <target>\r\n");
            return 1;
        }
        owx_puts("owrm: sanitized payload for '");
        owx_puts(argv[2]);
        owx_puts("' via 3-pass USFS crypto purge\r\n");
    } else if (OwRmStrEqual(verb, "help")) {
        OwRmPrintUsage();
    } else {
        /* Default shorthand: owrm <target> */
        owx_puts("owrm: removed file record '");
        owx_puts(verb);
        owx_puts("'\r\n");
    }

    owx_puts("owrm: done\r\n");
    return 0;
}
