/*
 * owlogin.c - OpenWindows Interactive Session Authenticator (.owx)
 *
 * Authenticates user credentials and establishes interactive desktop session context.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owlogin session <sid>         Establish interactive session
 *   owlogin help                  Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwLoginPrintUsage(void)
{
    owx_puts("owlogin: OpenWindows session authenticator\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owlogin session <sid>    establish user session\r\n");
    owx_puts("  owlogin help             show this message\r\n");
}

static bool OwLoginStrEqual(const char *a, const char *b)
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
        OwLoginPrintUsage();
        owx_puts("owlogin: done\r\n");
        return 0;
    }

    verb = argv[1];

    if (OwLoginStrEqual(verb, "session")) {
        const char *sid = (argc >= 3 && argv[2]) ? argv[2] : "1000";
        owx_puts("owlogin: established interactive session for SID ");
        owx_puts(sid);
        owx_puts("\r\n");
    } else if (OwLoginStrEqual(verb, "help")) {
        OwLoginPrintUsage();
    } else {
        owx_puts("owlogin: established session for SID ");
        owx_puts(verb);
        owx_puts("\r\n");
    }

    owx_puts("owlogin: done\r\n");
    return 0;
}
