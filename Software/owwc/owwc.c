/*
 * owwc.c - OpenWindows Word, Line & Byte Counter (.owx)
 *
 * Counts lines, words, and raw bytes of an OWFS file payload.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owwc count <file>             Count lines, words, and bytes
 *   owwc help                     Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWWC_BUFFER_SIZE 16384U
static char g_WcBuffer[OWWC_BUFFER_SIZE];

static void OwWcPrintUsage(void)
{
    owx_puts("owwc: OpenWindows file metrics counter\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owwc count <file>    count lines, words, and bytes\r\n");
    owx_puts("  owwc help            show this message\r\n");
}

static bool OwWcStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void OwWcCountFile(const char *path)
{
    int64_t bytes_read;
    int64_t i;
    uint64_t lines = 0;
    uint64_t words = 0;
    bool in_word = false;

    if (!path || !*path) {
        owx_puts("owwc: count: missing <file>\r\n");
        return;
    }

    bytes_read = owx_file_read(path, g_WcBuffer, sizeof(g_WcBuffer));
    if (bytes_read < 0) {
        owx_puts("owwc: failed to read '");
        owx_puts(path);
        owx_puts("'\r\n");
        return;
    }

    for (i = 0; i < bytes_read; i++) {
        char c = g_WcBuffer[i];
        if (c == '\n') lines++;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            words++;
        }
    }

    owx_puts("  Lines: ");
    owx_put_u64(lines);
    owx_puts("  Words: ");
    owx_put_u64(words);
    owx_puts("  Bytes: ");
    owx_put_u64((uint64_t)bytes_read);
    owx_puts("  File: ");
    owx_puts(path);
    owx_puts("\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwWcPrintUsage();
        owx_puts("owwc: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwWcStrEqual(verb, "count")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owwc: count: missing <file>\r\n");
            return 1;
        }
        OwWcCountFile(argv[2]);
    } else if (OwWcStrEqual(verb, "help")) {
        OwWcPrintUsage();
    } else {
        OwWcCountFile(verb);
    }

    owx_puts("owwc: done\r\n");
    return 0;
}
