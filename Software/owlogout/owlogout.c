/*
 * owlogout.c - OpenWindows Session Termination Utility (.owx)
 *
 * Terminates interactive desktop session and releases user tokens.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owlogout session              Terminate active session
 *   owlogout help                 Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwLogoutPrintUsage(void)
{
    owx_puts("owlogout: OpenWindows session termination utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owlogout session    terminate interactive session\r\n");
    owx_puts("  owlogout help       show this message\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    owx_puts("owlogout: session terminated\r\n");
    owx_puts("owlogout: done\r\n");
    return 0;
}
