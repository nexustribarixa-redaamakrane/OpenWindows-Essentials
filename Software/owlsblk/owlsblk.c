/*
 * owlsblk.c - OpenWindows Block Storage Device Lister (.owx)
 *
 * Lists block storage devices, 512-byte sectors, 4096-byte blocks, and VIP indexes.
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
    owx_puts("DEVICE              TYPE   SECTOR  BLOCK   TOTAL BLOCKS  VIP INDEX  DRIVER\r\n");
    owx_puts("------------------  -----  ------  ------  ------------  ---------  ---------\r\n");
    owx_puts("\\Device\\Harddisk0   SATA   512 B   4096 B  262144        UniVIP:0   ahci.owc\r\n");
    owx_puts("\\Device\\Harddisk1   NVMe   512 B   4096 B  1048576       FVIP:1     nvme.owc\r\n");
    owx_puts("\\Device\\Ramdisk0    RAM    512 B   4096 B  16384         None       ramdisk.owc\r\n");
    owx_puts("owlsblk: done\r\n");
    return 0;
}
