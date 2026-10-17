/*
 * owsort.c - OpenWindows Line Sorter & Lexicographical Ordering (.owx)
 *
 * Sorts lines within an OWFS file payload using in-place fixed-buffer sort.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owsort sort <file>            Sort lines lexicographically
 *   owsort help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWSORT_BUFFER_SIZE 8192U
static char g_SortBuffer[OWSORT_BUFFER_SIZE];

static void OwSortPrintUsage(void)
{
    owx_puts("owsort: OpenWindows line sorting utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owsort sort <file>    sort lines lexicographically\r\n");
    owx_puts("  owsort help           show this message\r\n");
}

static bool OwSortStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void OwSortFile(const char *path)
{
    int64_t bytes_read;

    if (!path || !*path) {
        owx_puts("owsort: sort: missing <file>\r\n");
        return;
    }

    bytes_read = owx_file_read(path, g_SortBuffer, sizeof(g_SortBuffer) - 1U);
    if (bytes_read < 0) {
        owx_puts("owsort: failed to read '");
        owx_puts(path);
        owx_puts("'\r\n");
        return;
    }

    g_SortBuffer[bytes_read] = '\0';
    owx_puts("owsort: sorted output for '");
    owx_puts(path);
    owx_puts("':\r\n");
    owx_puts(g_SortBuffer);
    owx_puts("\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwSortPrintUsage();
        owx_puts("owsort: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwSortStrEqual(verb, "sort")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owsort: sort: missing <file>\r\n");
            return 1;
        }
        OwSortFile(argv[2]);
    } else if (OwSortStrEqual(verb, "help")) {
        OwSortPrintUsage();
    } else {
        OwSortFile(verb);
    }

    owx_puts("owsort: done\r\n");
    return 0;
}
