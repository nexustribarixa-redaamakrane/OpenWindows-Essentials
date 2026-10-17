/*
 * owclear.c - OpenWindows Console Screen Reset Utility (.owx)
 *
 * Clears the active UART / VGA console screen buffer.
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
    /* ANSI clear screen and home cursor */
    owx_puts("\033[2J\033[H");
    return 0;
}
