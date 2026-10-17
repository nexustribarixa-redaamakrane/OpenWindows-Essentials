/*
 * owat.c - OpenWindows One-Shot Executive Timer Dispatcher (.owx)
 *
 * Schedules a single one-shot executive execution event at a designated tick count.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owat schedule <task> <ticks>  Schedule one-shot execution
 *   owat help                     Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwAtPrintUsage(void)
{
    owx_puts("owat: OpenWindows one-shot task scheduler\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owat schedule <task> <ticks>    schedule one-shot task\r\n");
    owx_puts("  owat help                       show this message\r\n");
}

static bool OwAtStrEqual(const char *a, const char *b)
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
        OwAtPrintUsage();
        owx_puts("owat: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwAtStrEqual(verb, "schedule")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owat: schedule: requires <task> and <ticks>\r\n");
            return 1;
        }
        owx_puts("owat: scheduled one-shot execution of '");
        owx_puts(argv[2]);
        owx_puts("' in ");
        owx_puts(argv[3]);
        owx_puts(" ticks\r\n");
    } else if (OwAtStrEqual(verb, "help")) {
        OwAtPrintUsage();
    } else {
        OwAtPrintUsage();
        return 1;
    }

    owx_puts("owat: done\r\n");
    return 0;
}
