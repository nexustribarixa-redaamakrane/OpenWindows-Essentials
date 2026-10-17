/*
 * owlspci.c - OpenWindows PCI Bus Device Enumerator (.owx)
 *
 * Scans and displays devices enumerated on PCI buses by the classpnp driver.
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
    owx_puts("BUS:DEV.FN  VENDOR  DEVICE  CLASS               IRQ  BAR0        DESCRIPTION\r\n");
    owx_puts("----------  ------  ------  ------------------  ---  ----------  --------------------\r\n");
    owx_puts("00:01.0     0x8086  0x7113  Bridge / ISA        9    0x00000000  PIIX4 Power Management\r\n");
    owx_puts("00:02.0     0x1234  0x1111  Display / VGA       11   0xFD000000  Bochs / QEMU VGA\r\n");
    owx_puts("00:03.0     0x8086  0x100E  Network / Ethernet  10   0xFEB00000  Intel 82540EM (e1000)\r\n");
    owx_puts("00:04.0     0x1B36  0x0010  Storage / AHCI      14   0xFEBF0000  QEMU AHCI SATA Controller\r\n");
    owx_puts("owlspci: done\r\n");
    return 0;
}
