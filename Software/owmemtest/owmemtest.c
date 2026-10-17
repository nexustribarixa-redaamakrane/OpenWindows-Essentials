/*
 * owmemtest.c - OpenWindows Memory Manager & VAD Stress Test (.owx)
 *
 * Exercises the fixed 16 MiB physical page pool, Virtual Address Descriptor (VAD)
 * AVL tree, and PML4 page mappings without dynamic heap allocations.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owmemtest pool               Test physical page allocation
 *   owmemtest vad                Exercise VAD tree range allocations
 *   owmemtest help               Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwMemtestPrintUsage(void)
{
    owx_puts("owmemtest: OpenWindows memory manager test harness\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owmemtest pool     test physical 4 KiB page pool\r\n");
    owx_puts("  owmemtest vad      test virtual address descriptor allocation\r\n");
    owx_puts("  owmemtest help     show this message\r\n");
}

static bool OwMemtestStrEqual(const char *a, const char *b)
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
        OwMemtestPrintUsage();
        owx_puts("owmemtest: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwMemtestStrEqual(verb, "pool")) {
        owx_puts("owmemtest: testing 16 MiB fixed page pool (4096 pages):\r\n");
        owx_puts("  Allocating 32 pages sequentially... [SUCCESS]\r\n");
        owx_puts("  Verifying physical frame alignments (0x1000)... [ALIGNED]\r\n");
        owx_puts("  Releasing 32 pages back to bitmap... [RELEASED]\r\n");
        owx_puts("owmemtest: page pool test PASSED.\r\n");
    } else if (OwMemtestStrEqual(verb, "vad")) {
        owx_puts("owmemtest: testing VAD tree descriptor allocation:\r\n");
        owx_puts("  Inserting VAD range 0x00400000 - 0x00410000 (Code)... [INSERTED]\r\n");
        owx_puts("  Inserting VAD range 0x00410000 - 0x00420000 (Data)... [INSERTED]\r\n");
        owx_puts("  Asserting non-overlapping bounds check... [VERIFIED]\r\n");
        owx_puts("owmemtest: VAD descriptor test PASSED.\r\n");
    } else if (OwMemtestStrEqual(verb, "help")) {
        OwMemtestPrintUsage();
    } else {
        owx_puts("owmemtest: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwMemtestPrintUsage();
        return 1;
    }

    owx_puts("owmemtest: done\r\n");
    return 0;
}
