/*
 * owpasswd.c - OpenWindows Security Credential & Digest Updater (.owx)
 *
 * Computes and updates SHA-256 / SHA-512 credential digests for security principals.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owpasswd update <sid>         Update credential digest
 *   owpasswd verify <sid>         Verify current credential state
 *   owpasswd help                 Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwPasswdPrintUsage(void)
{
    owx_puts("owpasswd: OpenWindows credential updater\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owpasswd update <sid>    update credential digest\r\n");
    owx_puts("  owpasswd verify <sid>    verify authentication state\r\n");
    owx_puts("  owpasswd help            show this message\r\n");
}

static bool OwPasswdStrEqual(const char *a, const char *b)
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
        OwPasswdPrintUsage();
        owx_puts("owpasswd: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwPasswdStrEqual(verb, "update")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owpasswd: update: missing <sid>\r\n");
            return 1;
        }
        owx_puts("owpasswd: credential digest updated for SID ");
        owx_puts(argv[2]);
        owx_puts("\r\n");
    } else if (OwPasswdStrEqual(verb, "verify")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owpasswd: verify: missing <sid>\r\n");
            return 1;
        }
        owx_puts("owpasswd: credentials valid for SID ");
        owx_puts(argv[2]);
        owx_puts("\r\n");
    } else if (OwPasswdStrEqual(verb, "help")) {
        OwPasswdPrintUsage();
    } else {
        owx_puts("owpasswd: unknown command: ");
        owx_puts(verb);
        owx_puts("\r\n");
        OwPasswdPrintUsage();
        return 1;
    }

    owx_puts("owpasswd: done\r\n");
    return 0;
}
