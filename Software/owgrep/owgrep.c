/*
 * owgrep.c - OpenWindows Pattern & String Search Utility (.owx)
 *
 * Searches file payloads for matching substrings and SUTF-8 patterns.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owgrep find <pattern> <file>      Search and print matching lines
 *   owgrep count <pattern> <file>     Print total matching line count
 *   owgrep help                       Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWGREP_BUFFER_SIZE 16384U
static char g_FileBuffer[OWGREP_BUFFER_SIZE];

static void OwGrepPrintUsage(void)
{
    owx_puts("owgrep: OpenWindows pattern search utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owgrep find <pattern> <file>     print lines matching pattern\r\n");
    owx_puts("  owgrep count <pattern> <file>    count matching occurrences\r\n");
    owx_puts("  owgrep help                      show this message\r\n");
}

static bool OwGrepStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static bool OwGrepContains(const char *haystack, uint32_t len, const char *needle)
{
    uint32_t n_len = 0;
    uint32_t i, j;

    if (!haystack || !needle) return false;
    while (needle[n_len]) n_len++;
    if (n_len == 0) return true;
    if (n_len > len) return false;

    for (i = 0; i <= len - n_len; i++) {
        bool match = true;
        for (j = 0; j < n_len; j++) {
            if (haystack[i + j] != needle[j]) {
                match = false;
                break;
            }
        }
        if (match) return true;
    }

    return false;
}

static void OwGrepSearch(const char *pattern, const char *file, bool count_only)
{
    int64_t bytes_read;
    int64_t pos = 0;
    int64_t line_start = 0;
    uint64_t line_num = 1;
    uint64_t match_count = 0;

    if (!pattern || !file) {
        owx_puts("owgrep: search: requires <pattern> and <file>\r\n");
        return;
    }

    bytes_read = owx_file_read(file, g_FileBuffer, sizeof(g_FileBuffer) - 1U);
    if (bytes_read < 0) {
        owx_puts("owgrep: failed to read '");
        owx_puts(file);
        owx_puts("'\r\n");
        return;
    }
    g_FileBuffer[bytes_read] = '\0';

    while (pos <= bytes_read) {
        if (pos == bytes_read || g_FileBuffer[pos] == '\n') {
            uint32_t line_len = (uint32_t)(pos - line_start);
            if (line_len > 0 && g_FileBuffer[line_start + line_len - 1] == '\r') {
                line_len--;
            }

            if (OwGrepContains(&g_FileBuffer[line_start], line_len, pattern)) {
                match_count++;
                if (!count_only) {
                    owx_puts("[Line ");
                    owx_put_u64(line_num);
                    owx_puts("] ");
                    for (uint32_t k = 0; k < line_len; k++) {
                        owx_putc(g_FileBuffer[line_start + k]);
                    }
                    owx_puts("\r\n");
                }
            }

            line_num++;
            line_start = pos + 1;
        }
        pos++;
    }

    if (count_only) {
        owx_puts("owgrep: occurrences: ");
        owx_put_u64(match_count);
        owx_puts("\r\n");
    }
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwGrepPrintUsage();
        owx_puts("owgrep: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwGrepStrEqual(verb, "find")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owgrep: find: requires <pattern> and <file>\r\n");
            return 1;
        }
        OwGrepSearch(argv[2], argv[3], false);
    } else if (OwGrepStrEqual(verb, "count")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owgrep: count: requires <pattern> and <file>\r\n");
            return 1;
        }
        OwGrepSearch(argv[2], argv[3], true);
    } else if (OwGrepStrEqual(verb, "help")) {
        OwGrepPrintUsage();
    } else {
        if (argc >= 3 && argv[2]) {
            OwGrepSearch(verb, argv[2], false);
        } else {
            OwGrepPrintUsage();
            return 1;
        }
    }

    owx_puts("owgrep: done\r\n");
    return 0;
}
