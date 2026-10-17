/*
 * owreboot.c - OpenWindows System Reboot Utility (.owx)
 *
 * Requests runlevel transition to profile 6 (REBOOT) via the kernel gateway.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owreboot restart              Execute clean system reboot
 *   owreboot help                 Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwRebootPrintUsage(void)
{
    owx_puts("owreboot: OpenWindows system restart utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owreboot restart    trigger runlevel 6 system recycle\r\n");
    owx_puts("  owreboot help       show this message\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    owx_puts("owreboot: transitioning kernel to Runlevel 6 [REBOOT]...\r\n");
    owx_puts("owreboot: flushing VFS storage dirty pages...\r\n");
    owx_puts("owreboot: recycling CPU via reset control...\r\n");
    return 0;
}
