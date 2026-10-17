/*
 * owhead.c - OpenWindows File Header Line Viewer (.owx)
 *
 * Reads and displays the initial lines of an OWFS file payload.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owhead read <file> [lines]    Display first n lines of file
 *   owhead help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWHEAD_BUFFER_SIZE 4096U
static char g_HeadBuffer[OWHEAD_BUFFER_SIZE];

static void OwHeadPrintUsage(void)
{
    owx_puts("owhead: OpenWindows file header line viewer\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owhead read <file> [lines]    stream first n lines\r\n");
    owx_puts("  owhead help                   show this message\r\n");
}

static bool OwHeadStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static uint32_t OwHeadParseDec(const char *s)
{
    uint32_t val = 0;
    if (!s) return 0;
    while (*s >= '0' && *s <= '9') {
        val = (val * 10) + (uint32_t)(*s - '0');
        s++;
    }
    return val;
}

static void OwHeadReadFile(const char *path, uint32_t max_lines)
{
    int64_t bytes_read;
    int64_t pos = 0;
    uint32_t lines_seen = 0;

    if (!path || !*path) {
        owx_puts("owhead: read: missing <file>\r\n");
        return;
    }

    bytes_read = owx_file_read(path, g_HeadBuffer, sizeof(g_HeadBuffer) - 1U);
    if (bytes_read < 0) {
        owx_puts("owhead: failed to read '");
        owx_puts(path);
        owx_puts("'\r\n");
        return;
    }

    while (pos < bytes_read && lines_seen < max_lines) {
        owx_putc(g_HeadBuffer[pos]);
        if (g_HeadBuffer[pos] == '\n') {
            lines_seen++;
        }
        pos++;
    }
    owx_puts("\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwHeadPrintUsage();
        owx_puts("owhead: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwHeadStrEqual(verb, "read")) {
        uint32_t lines = 10;
        if (argc < 3 || !argv[2]) {
            owx_puts("owhead: read: missing <file>\r\n");
            return 1;
        }
        if (argc >= 4 && argv[3]) {
            lines = OwHeadParseDec(argv[3]);
            if (lines == 0) lines = 10;
        }
        OwHeadReadFile(argv[2], lines);
    } else if (OwHeadStrEqual(verb, "help")) {
        OwHeadPrintUsage();
    } else {
        uint32_t lines = 10;
        if (argc >= 3 && argv[2]) {
            lines = OwHeadParseDec(argv[2]);
            if (lines == 0) lines = 10;
        }
        OwHeadReadFile(verb, lines);
    }

    owx_puts("owhead: done\r\n");
    return 0;
}
