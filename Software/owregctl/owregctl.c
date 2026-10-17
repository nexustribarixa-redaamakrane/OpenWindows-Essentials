/*
 * owregctl.c - OpenWindows Configuration Tree & Registry Control CLI (.owx)
 *
 * Controls hierarchical configuration keys and system registry entries
 * backed by the kconf64 library.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owregctl query <key>          Query configuration key value
 *   owregctl set <key> <value>    Set configuration key value
 *   owregctl count                Count total registered keys
 *   owregctl help                 Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"
#include "../../DLL/kconf64/kconf64.h"

static void OwRegctlPrintUsage(void)
{
    owx_puts("owregctl: OpenWindows configuration tree control\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owregctl query <key>         print key value\r\n");
    owx_puts("  owregctl set <key> <val>     set key value (in-memory)\r\n");
    owx_puts("  owregctl count               display total keys\r\n");
    owx_puts("  owregctl help                show this message\r\n");
}

static bool OwRegctlStrEqual(const char *a, const char *b)
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
        OwRegctlPrintUsage();
        owx_puts("owregctl: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwRegctlStrEqual(verb, "query")) {
        const char *val;
        if (argc < 3 || !argv[2]) {
            owx_puts("owregctl: query: missing <key>\r\n");
            return 1;
        }
        val = kconf64_get_string(argv[2], "(null)");
        owx_puts("owregctl: ");
        owx_puts(argv[2]);
        owx_puts(" = ");
        owx_puts(val);
        owx_puts("\r\n");
    } else if (OwRegctlStrEqual(verb, "set")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owregctl: set: requires <key> and <value>\r\n");
            return 1;
        }
        kconf64_set_string(argv[2], argv[3]);
        owx_puts("owregctl: set ");
        owx_puts(argv[2]);
        owx_puts(" = ");
        owx_puts(argv[3]);
        owx_puts(" (in-memory)\r\n");
    } else if (OwRegctlStrEqual(verb, "count")) {
        owx_puts("owregctl: registered keys: ");
        owx_put_u64((uint64_t)kconf64_count());
        owx_puts("\r\n");
    } else if (OwRegctlStrEqual(verb, "help")) {
        OwRegctlPrintUsage();
    } else {
        owx_puts("owregctl: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwRegctlPrintUsage();
        return 1;
    }

    owx_puts("owregctl: done\r\n");
    return 0;
}
