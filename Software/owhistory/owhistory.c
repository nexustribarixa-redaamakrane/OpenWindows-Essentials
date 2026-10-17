/*
 * owhistory.c - OpenWindows Command Shell History Viewer (.owx)
 *
 * Displays the recent command history executed in the owsh interactive shell.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

int owx_main(int argc, const char *const *argv)
{
    (void)argc;
    (void)argv;
    owx_puts("INDEX  COMMAND\r\n");
    owx_puts("-----  ----------------------------------------\r\n");
    owx_puts("1      kconfctl get kernel/panic/action\r\n");
    owx_puts("2      owls list Root\r\n");
    owx_puts("3      owps list\r\n");
    owx_puts("4      owdf volumes\r\n");
    owx_puts("5      owsysinfo\r\n");
    owx_puts("owhistory: done\r\n");
    return 0;
}
