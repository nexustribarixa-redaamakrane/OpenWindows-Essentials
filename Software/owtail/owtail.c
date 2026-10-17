/*
 * owtail.c - OpenWindows File Tail Line Viewer (.owx)
 *
 * Reads and displays trailing lines of an OWFS file payload.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owtail read <file> [lines]    Display last n lines of file
 *   owtail help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWTAIL_BUFFER_SIZE 4096U
static char g_TailBuffer[OWTAIL_BUFFER_SIZE];

static void OwTailPrintUsage(void)
{
    owx_puts("owtail: OpenWindows file tail line viewer\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owtail read <file> [lines]    stream last n lines\r\n");
    owx_puts("  owtail help                   show this message\r\n");
}

static bool OwTailStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static uint32_t OwTailParseDec(const char *s)
{
    uint32_t val = 0;
    if (!s) return 0;
    while (*s >= '0' && *s <= '9') {
        val = (val * 10) + (uint32_t)(*s - '0');
        s++;
    }
    return val;
}

static void OwTailReadFile(const char *path, uint32_t max_lines)
{
    int64_t bytes_read;
    int64_t pos;
    uint32_t lines_seen = 0;
    int64_t start_pos = 0;

    if (!path || !*path) {
        owx_puts("owtail: read: missing <file>\r\n");
        return;
    }

    bytes_read = owx_file_read(path, g_TailBuffer, sizeof(g_TailBuffer) - 1U);
    if (bytes_read < 0) {
        owx_puts("owtail: failed to read '");
        owx_puts(path);
        owx_puts("'\r\n");
        return;
    }

    /* Walk backwards from end to count lines */
    for (pos = bytes_read - 1; pos >= 0; pos--) {
        if (g_TailBuffer[pos] == '\n') {
            lines_seen++;
            if (lines_seen > max_lines) {
                start_pos = pos + 1;
                break;
            }
        }
    }

    for (pos = start_pos; pos < bytes_read; pos++) {
        owx_putc(g_TailBuffer[pos]);
    }
    owx_puts("\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwTailPrintUsage();
        owx_puts("owtail: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwTailStrEqual(verb, "read")) {
        uint32_t lines = 10;
        if (argc < 3 || !argv[2]) {
            owx_puts("owtail: read: missing <file>\r\n");
            return 1;
        }
        if (argc >= 4 && argv[3]) {
            lines = OwTailParseDec(argv[3]);
            if (lines == 0) lines = 10;
        }
        OwTailReadFile(argv[2], lines);
    } else if (OwTailStrEqual(verb, "help")) {
        OwTailPrintUsage();
    } else {
        uint32_t lines = 10;
        if (argc >= 3 && argv[2]) {
            lines = OwTailParseDec(argv[2]);
            if (lines == 0) lines = 10;
        }
        OwTailReadFile(verb, lines);
    }

    owx_puts("owtail: done\r\n");
    return 0;
}
