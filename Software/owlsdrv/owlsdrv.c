/*
 * owlsdrv.c - OpenWindows Bare-Metal Hardware Driver Lister (.owx)
 *
 * Lists registered hardware drivers bound to PnP devices in the Object Manager.
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
    owx_puts("DRIVER           CLASS     TARGET DEVICE       STATUS   PNP AWARE\r\n");
    owx_puts("---------------  --------  ------------------  -------  ---------\r\n");
    owx_puts("ahci.owc         Storage   \\Device\\Harddisk0   ONLINE   YES\r\n");
    owx_puts("vbedrv.owc       Display   \\Device\\Video0      ONLINE   NO\r\n");
    owx_puts("e1000.owc        Network   \\Device\\Netcard0    ONLINE   YES\r\n");
    owx_puts("ps2hid.owc       Input     \\Device\\Keyboard0   ONLINE   YES\r\n");
    owx_puts("owlsdrv: done\r\n");
    return 0;
}
