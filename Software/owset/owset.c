/*
 * owset.c - OpenWindows Session Environment Setter (.owx)
 *
 * Sets and updates session environment variables and configuration properties.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owset set <var> <value>      Set environment variable
 *   owset clear <var>            Clear environment variable
 *   owset help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../DLL/kconf64/kconf64.h"

static void OwSetPrintUsage(void)
{
    owx_puts("owset: OpenWindows session environment setter\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owset set <var> <val>     set environment variable\r\n");
    owx_puts("  owset clear <var>         clear environment variable\r\n");
    owx_puts("  owset help                show this message\r\n");
}

static bool OwSetStrEqual(const char *a, const char *b)
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
        OwSetPrintUsage();
        owx_puts("owset: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwSetStrEqual(verb, "set")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owset: set: requires <var> and <val>\r\n");
            return 1;
        }
        kconf64_set_string(argv[2], argv[3]);
        owx_puts("owset: set ");
        owx_puts(argv[2]);
        owx_puts("=");
        owx_puts(argv[3]);
        owx_puts("\r\n");
    } else if (OwSetStrEqual(verb, "clear")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owset: clear: missing <var>\r\n");
            return 1;
        }
        kconf64_set_string(argv[2], "");
        owx_puts("owset: cleared ");
        owx_puts(argv[2]);
        owx_puts("\r\n");
    } else if (OwSetStrEqual(verb, "help")) {
        OwSetPrintUsage();
    } else {
        if (argc >= 3 && argv[2]) {
            kconf64_set_string(verb, argv[2]);
            owx_puts("owset: set ");
            owx_puts(verb);
            owx_puts("=");
            owx_puts(argv[2]);
            owx_puts("\r\n");
        } else {
            OwSetPrintUsage();
            return 1;
        }
    }

    owx_puts("owset: done\r\n");
    return 0;
}
