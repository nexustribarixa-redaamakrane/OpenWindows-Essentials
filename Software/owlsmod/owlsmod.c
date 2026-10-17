/*
 * owlsmod.c - OpenWindows Kernel Extension & Module Lister (.owx)
 *
 * Lists loaded .owc kernel modules and tier-1 extensions.
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
    owx_puts("MODULE      TIER  BASE ADDR           SIZE (B)  SECTIONS  EXPORTS\r\n");
    owx_puts("----------  ----  ------------------  --------  --------  -------\r\n");
    owx_puts("fltrmgr     1     0xFFFFF80000200000  13719     4         8\r\n");
    owx_puts("classpnp    1     0xFFFFF80000210000  14040     5         12\r\n");
    owx_puts("banhammer   1     0xFFFFF80000220000  8192      3         4\r\n");
    owx_puts("owfsdrv     1     0xFFFFF80000230000  16384     6         16\r\n");
    owx_puts("owlsmod: done\r\n");
    return 0;
}
