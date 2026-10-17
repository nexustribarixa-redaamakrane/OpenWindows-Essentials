/*
 * owcat.c - OpenWindows File Streaming & Inspection Utility (.owx)
 *
 * Reads and streams file contents from the primary OWFS storage volume
 * through the Ring 3 system call gateway (owx_file_read).
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owcat read <file>             Stream file contents to console
 *   owcat dump <file>             Hexadecimal stream inspector
 *   owcat help                    Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWCAT_BUFFER_SIZE 4096U

static char g_FileBuffer[OWCAT_BUFFER_SIZE];

static void OwCatPrintUsage(void)
{
    owx_puts("owcat: OpenWindows file streaming utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owcat read <file>    stream file content to console\r\n");
    owx_puts("  owcat dump <file>    hexadecimal byte stream inspector\r\n");
    owx_puts("  owcat help           show this message\r\n");
}

static bool OwCatStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void OwCatReadFile(const char *path)
{
    int64_t bytes_read;

    if (!path || !*path) {
        owx_puts("owcat: read: missing <file>\r\n");
        return;
    }

    bytes_read = owx_file_read(path, g_FileBuffer, (uint32_t)sizeof(g_FileBuffer) - 1U);
    if (bytes_read < 0) {
        owx_puts("owcat: failed to read '");
        owx_puts(path);
        owx_puts("' (error code: ");
        owx_put_i64(bytes_read);
        owx_puts(")\r\n");
        return;
    }

    g_FileBuffer[bytes_read] = '\0';
    owx_puts(g_FileBuffer);
    owx_puts("\r\n");
}

static void OwCatDumpHex(const char *path)
{
    int64_t bytes_read;
    int64_t i;

    if (!path || !*path) {
        owx_puts("owcat: dump: missing <file>\r\n");
        return;
    }

    bytes_read = owx_file_read(path, g_FileBuffer, (uint32_t)sizeof(g_FileBuffer));
    if (bytes_read < 0) {
        owx_puts("owcat: failed to read '");
        owx_puts(path);
        owx_puts("'\r\n");
        return;
    }

    owx_puts("owcat: hex dump for '");
    owx_puts(path);
    owx_puts("' (");
    owx_put_u64((uint64_t)bytes_read);
    owx_puts(" bytes):\r\n");

    for (i = 0; i < bytes_read; i++) {
        uint8_t byte = (uint8_t)g_FileBuffer[i];
        static const char hex_digits[] = "0123456789ABCDEF";
        owx_putc(hex_digits[(byte >> 4) & 0x0F]);
        owx_putc(hex_digits[byte & 0x0F]);
        owx_putc(' ');
        if ((i + 1) % 16 == 0) {
            owx_puts("\r\n");
        }
    }
    owx_puts("\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwCatPrintUsage();
        owx_puts("owcat: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwCatStrEqual(verb, "read")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owcat: read: missing <file>\r\n");
            return 1;
        }
        OwCatReadFile(argv[2]);
    } else if (OwCatStrEqual(verb, "dump")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owcat: dump: missing <file>\r\n");
            return 1;
        }
        OwCatDumpHex(argv[2]);
    } else if (OwCatStrEqual(verb, "help")) {
        OwCatPrintUsage();
    } else {
        /* Default shorthand: owcat <file> */
        OwCatReadFile(verb);
    }

    owx_puts("owcat: done\r\n");
    return 0;
}
