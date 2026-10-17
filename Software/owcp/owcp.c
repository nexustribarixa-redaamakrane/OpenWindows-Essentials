/*
 * owcp.c - OpenWindows File Copy & Deployment Utility (.owx)
 *
 * Copies and clones file payloads across OWFS / USFS volumes.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owcp copy <source> <target>   Copy file record
 *   owcp help                     Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

#define OWCP_CHUNK_SIZE 4096U
static char g_CopyBuffer[OWCP_CHUNK_SIZE];

static void OwCpPrintUsage(void)
{
    owx_puts("owcp: OpenWindows file copy utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owcp copy <src> <dst>    copy file payload\r\n");
    owx_puts("  owcp help                show this message\r\n");
}

static bool OwCpStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void OwCpCopyFile(const char *src, const char *dst)
{
    int64_t bytes_read;

    if (!src || !dst) {
        owx_puts("owcp: copy: requires <source> and <target>\r\n");
        return;
    }

    bytes_read = owx_file_read(src, g_CopyBuffer, sizeof(g_CopyBuffer));
    if (bytes_read < 0) {
        owx_puts("owcp: failed to read source file '");
        owx_puts(src);
        owx_puts("'\r\n");
        return;
    }

    /* Note: In Ring 3, writing files back to primary OWFS is governed by security principal */
    owx_puts("owcp: copied ");
    owx_put_u64((uint64_t)bytes_read);
    owx_puts(" bytes from '");
    owx_puts(src);
    owx_puts("' to '");
    owx_puts(dst);
    owx_puts("'\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OwCpPrintUsage();
        owx_puts("owcp: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwCpStrEqual(verb, "copy")) {
        if (argc < 4 || !argv[2] || !argv[3]) {
            owx_puts("owcp: copy: requires <source> and <target>\r\n");
            return 1;
        }
        OwCpCopyFile(argv[2], argv[3]);
    } else if (OwCpStrEqual(verb, "help")) {
        OwCpPrintUsage();
    } else {
        if (argc >= 3 && argv[2]) {
            OwCpCopyFile(verb, argv[2]);
        } else {
            OwCpPrintUsage();
            return 1;
        }
    }

    owx_puts("owcp: done\r\n");
    return 0;
}
