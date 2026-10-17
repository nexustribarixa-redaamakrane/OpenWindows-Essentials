/*
 * owtextdiff.c - OpenWindows File Comparison Utility (.owx)
 *
 * Compares two file records on OWFS storage and reports differences.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owtextdiff compare <file1> <file2>    Compare two file payloads
 *   owtextdiff help                       Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWDIFF_BUF_SIZE 2048U
static char g_File1Buf[OWDIFF_BUF_SIZE];
static char g_File2Buf[OWDIFF_BUF_SIZE];

static void OwDiffPrintUsage(void)
{
    owx_puts("owtextdiff: OpenWindows file comparison utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owtextdiff compare <file1> <file2>    compare files\r\n");
    owx_puts("  owtextdiff help                       show this message\r\n");
}

static bool OwDiffStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void OwDiffCompareFiles(const char *f1, const char *f2)
{
    int64_t n1, n2;
    int64_t i;
    bool identical = true;

    if (!f1 || !f2) {
        owx_puts("owtextdiff: compare: requires two files\r\n");
        return;
    }

    n1 = owx_file_read(f1, g_File1Buf, sizeof(g_File1Buf));
    n2 = owx_file_read(f2, g_File2Buf, sizeof(g_File2Buf));

    if (n1 < 0 || n2 < 0) {
        owx_puts("owtextdiff: error reading comparison targets\r\n");
        return;
    }

    if (n1 != n2) {
        owx_puts("owtextdiff: file sizes differ (");
        owx_put_u64((uint64_t)n1);
        owx_puts(" vs ");
        owx_put_u64((uint64_t)n2);
        owx_puts(" bytes)\r\n");
        identical = false;
    } else {
        for (i = 0; i < n1; i++) {
            if (g_File1Buf[i] != g_File2Buf[i]) {
                owx_puts("owtextdiff: byte mismatch at offset 0x");
                owx_put_u64((uint64_t)i);
                owx_puts("\r\n");
                identical = false;
                break;
            }
        }
    }

    if (identical) {
        owx_puts("owtextdiff: files are byte-for-byte IDENTICAL\r\n");
    }
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwDiffPrintUsage();
        owx_puts("owtextdiff: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwDiffStrEqual(verb, "compare")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owtextdiff: compare: requires <file1> and <file2>\r\n");
            return 1;
        }
        OwDiffCompareFiles(argv[2], argv[3]);
    } else if (OwDiffStrEqual(verb, "help")) {
        OwDiffPrintUsage();
    } else {
        if (argc >= 3 && argv[2]) {
            OwDiffCompareFiles(verb, argv[2]);
        } else {
            OwDiffPrintUsage();
            return 1;
        }
    }

    owx_puts("owtextdiff: done\r\n");
    return 0;
}
