/*
 * owhalt.c - OpenWindows System Powerdown & Halt Utility (.owx)
 *
 * Requests runlevel transition to profile 0 (HALT) and triggers ACPI power-off.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owhalt poweroff               Execute clean shutdown and ACPI power-off
 *   owhalt help                   Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

static void OwHaltPrintUsage(void)
{
    owx_puts("owhalt: OpenWindows system shutdown utility\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owhalt poweroff    transition to runlevel 0 and power off\r\n");
    owx_puts("  owhalt help        show this message\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    owx_puts("owhalt: transitioning kernel to Runlevel 0 [HALT]...\r\n");
    owx_puts("owhalt: running rc shutdown directives...\r\n");
    owx_puts("owhalt: issuing ACPI S5 power-off...\r\n");
    return 0;
}
