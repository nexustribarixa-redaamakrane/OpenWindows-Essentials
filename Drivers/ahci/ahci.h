/*
 * ahci.h - OpenWindows Advanced Host Controller Interface (SATA) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef AHCI_H
#define AHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void ahci_init(void);
void ahci_probe_ports(void);
void ahci_read_sectors(void);
void ahci_write_sectors(void);

#endif /* AHCI_H */

