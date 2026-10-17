/*
 * owhexdump.c - OpenWindows Binary Block & Payload Hex Dump Utility (.owx)
 *
 * Formats and displays binary payloads in 16-byte hex rows with ASCII columns.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owhexdump dump <file>         Hexadecimal dump of file payload
 *   owhexdump help                Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWHEX_BUFFER_SIZE 4096U
static char g_HexBuffer[OWHEX_BUFFER_SIZE];

static void OwHexPrintUsage(void)
{
    owx_puts("owhexdump: OpenWindows binary block dump utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owhexdump dump <file>    hexadecimal block dump\r\n");
    owx_puts("  owhexdump help           show this message\r\n");
}

static bool OwHexStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void OwHexDumpFile(const char *path)
{
    int64_t bytes_read;
    int64_t i, j;
    static const char hex_digits[] = "0123456789ABCDEF";

    if (!path || !*path) {
        owx_puts("owhexdump: dump: missing <file>\r\n");
        return;
    }

    bytes_read = owx_file_read(path, g_HexBuffer, sizeof(g_HexBuffer));
    if (bytes_read < 0) {
        owx_puts("owhexdump: failed to read '");
        owx_puts(path);
        owx_puts("'\r\n");
        return;
    }

    owx_puts("OFFSET    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ASCII\r\n");
    owx_puts("--------  -------------------------------------------------  ----------------\r\n");

    for (i = 0; i < bytes_read; i += 16) {
        /* Print offset */
        for (j = 28; j >= 0; j -= 4) {
            owx_putc(hex_digits[((uint32_t)i >> j) & 0x0F]);
        }
        owx_puts("  ");

        /* Print hex bytes */
        for (j = 0; j < 16; j++) {
            if (i + j < bytes_read) {
                uint8_t b = (uint8_t)g_HexBuffer[i + j];
                owx_putc(hex_digits[(b >> 4) & 0x0F]);
                owx_putc(hex_digits[b & 0x0F]);
                owx_putc(' ');
            } else {
                owx_puts("   ");
            }
            if (j == 7) owx_putc(' ');
        }

        owx_puts(" |");
        /* Print ASCII */
        for (j = 0; j < 16 && (i + j) < bytes_read; j++) {
            char c = g_HexBuffer[i + j];
            if (c >= 32 && c <= 126) {
                owx_putc(c);
            } else {
                owx_putc('.');
            }
        }
        owx_puts("|\r\n");
    }
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwHexPrintUsage();
        owx_puts("owhexdump: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwHexStrEqual(verb, "dump")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owhexdump: dump: missing <file>\r\n");
            return 1;
        }
        OwHexDumpFile(argv[2]);
    } else if (OwHexStrEqual(verb, "help")) {
        OwHexPrintUsage();
    } else {
        OwHexDumpFile(verb);
    }

    owx_puts("owhexdump: done\r\n");
    return 0;
}
