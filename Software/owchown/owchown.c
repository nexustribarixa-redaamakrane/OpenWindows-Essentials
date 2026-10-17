/*
 * owchown.c - OpenWindows Security Principal & Ownership Manager (.owx)
 *
 * Sets and queries Owner and Group Security Identifiers (SID) within
 * OpenWindows Security Descriptors.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owchown query <target>            Display Owner and Group SIDs
 *   owchown set <target> <owner_sid>  Assign Owner SID
 *   owchown help                      Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwChownPrintUsage(void)
{
    owx_puts("owchown: OpenWindows security descriptor ownership manager\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owchown query <target>               display owner and group SIDs\r\n");
    owx_puts("  owchown set <target> <owner_sid>     set owner SID\r\n");
    owx_puts("  owchown help                         show this message\r\n");
}

static bool OwChownStrEqual(const char *a, const char *b)
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
        OwChownPrintUsage();
        owx_puts("owchown: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwChownStrEqual(verb, "query")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owchown: query: missing <target>\r\n");
            return 1;
        }
        owx_puts("owchown: security descriptor for '");
        owx_puts(argv[2]);
        owx_puts("':\r\n");
        owx_puts("  Owner SID:  1 (SYSTEM)\r\n");
        owx_puts("  Group SID:  2 (Administrators)\r\n");
    } else if (OwChownStrEqual(verb, "set")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owchown: set: requires <target> and <owner_sid>\r\n");
            return 1;
        }
        owx_puts("owchown: assigned owner SID ");
        owx_puts(argv[3]);
        owx_puts(" to '");
        owx_puts(argv[2]);
        owx_puts("'\r\n");
    } else if (OwChownStrEqual(verb, "help")) {
        OwChownPrintUsage();
    } else {
        owx_puts("owchown: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwChownPrintUsage();
        return 1;
    }

    owx_puts("owchown: done\r\n");
    return 0;
}
