/*
 * owsleep.c - OpenWindows Scheduler Quantum Delay Utility (.owx)
 *
 * Suspends thread execution for a specified duration of 100 Hz PIT timer ticks
 * using the Ring 3 gateway (owx_sleep).
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owsleep ticks <count>         Delay by scheduler ticks (10 ms per tick)
 *   owsleep seconds <count>       Delay by whole seconds
 *   owsleep help                  Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwSleepPrintUsage(void)
{
    owx_puts("owsleep: OpenWindows scheduler delay utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owsleep ticks <n>      delay by n scheduler ticks (100 Hz)\r\n");
    owx_puts("  owsleep seconds <n>    delay by n seconds\r\n");
    owx_puts("  owsleep help           show this message\r\n");
}

static bool OwSleepStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static int64_t OwSleepParseDec(const char *s)
{
    int64_t val = 0;
    if (!s) return 0;
    while (*s >= '0' && *s <= '9') {
        val = (val * 10) + (int64_t)(*s - '0');
        s++;
    }
    return val;
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwSleepPrintUsage();
        owx_puts("owsleep: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwSleepStrEqual(verb, "ticks")) {
        int64_t ticks = (argc >= 3 && argv[2]) ? OwSleepParseDec(argv[2]) : 10;
        owx_sleep(ticks);
    } else if (OwSleepStrEqual(verb, "seconds")) {
        int64_t secs = (argc >= 3 && argv[2]) ? OwSleepParseDec(argv[2]) : 1;
        owx_sleep(secs * 100);
    } else if (OwSleepStrEqual(verb, "help")) {
        OwSleepPrintUsage();
    } else {
        /* Default: treat argument as tick count */
        int64_t t = OwSleepParseDec(verb);
        owx_sleep(t > 0 ? t : 10);
    }

    owx_puts("owsleep: done\r\n");
    return 0;
}
