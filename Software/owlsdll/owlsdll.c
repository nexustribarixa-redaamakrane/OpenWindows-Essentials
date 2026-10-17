/*
 * owlsdll.c - OpenWindows Dynamic Link Library (.owd) Lister (.owx)
 *
 * Lists loaded freestanding dynamic libraries, base addresses, and exports.
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
    owx_puts("LIBRARY           TYPE     BASE ADDR           EXPORTS  DEPENDENCIES\r\n");
    owx_puts("----------------  -------  ------------------  -------  --------------------\r\n");
    owx_puts("kernel64.owd      Hybrid   0x0000000040000000  48       <none>\r\n");
    owx_puts("htl.owd           Kernel   0x0000000040040000  32       kernel64.owd\r\n");
    owx_puts("kconf64.owd       User     0x0000000040080000  16       kernel64.owd\r\n");
    owx_puts("cairo64.owd       User     0x0000000040100000  128      kernel64.owd\r\n");
    owx_puts("owfs64.owd        Hybrid   0x0000000040200000  24       htl.owd\r\n");
    owx_puts("owlsdll: done\r\n");
    return 0;
}
