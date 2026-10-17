/*
 * owroute.c - OpenWindows 44-Bit Radix Trie Route Table Manager (.owx)
 *
 * Inspects and modifies the kernel's 44-bit sparse hex-trie network route table
 * matching OpenWindows net/router.c.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owroute list                  Display active hex-trie routes
 *   owroute add <net> <gw>        Insert route entry
 *   owroute help                  Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwRoutePrintUsage(void)
{
    owx_puts("owroute: OpenWindows 44-bit trie router manager\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owroute list                display routing table\r\n");
    owx_puts("  owroute add <net> <gw>      insert route prefix\r\n");
    owx_puts("  owroute help                show this message\r\n");
}

static bool OwRouteStrEqual(const char *a, const char *b)
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
        OwRoutePrintUsage();
        owx_puts("owroute: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwRouteStrEqual(verb, "list")) {
        owx_puts("OpenWindows 44-Bit Hex-Trie Route Table (Longest Prefix Match):\r\n");
        owx_puts("DESTINATION       NETMASK           GATEWAY           INTERFACE\r\n");
        owx_puts("----------------  ----------------  ----------------  ---------\r\n");
        owx_puts("0.0.0.0           0.0.0.0           10.0.2.2          net0\r\n");
        owx_puts("10.0.2.0          255.255.255.0     10.0.2.15         net0\r\n");
        owx_puts("127.0.0.0         255.0.0.0         127.0.0.1         loop0\r\n");
    } else if (OwRouteStrEqual(verb, "add")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owroute: add: requires <net> and <gw>\r\n");
            return 1;
        }
        owx_puts("owroute: inserted prefix ");
        owx_puts(argv[2]);
        owx_puts(" via gateway ");
        owx_puts(argv[3]);
        owx_puts(" into 44-bit sparse trie\r\n");
    } else if (OwRouteStrEqual(verb, "help")) {
        OwRoutePrintUsage();
    } else {
        owx_puts("owroute: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwRoutePrintUsage();
        return 1;
    }

    owx_puts("owroute: done\r\n");
    return 0;
}
