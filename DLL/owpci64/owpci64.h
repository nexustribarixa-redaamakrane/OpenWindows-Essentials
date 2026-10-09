/*
 * owpci64.h - OpenWindows PCI Configuration Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPCI64_H
#define OWPCI64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owpci_read8(void);
void owpci_read16(void);
void owpci_read32(void);
void owpci_write8(void);
void owpci_write16(void);
void owpci_write32(void);
void owpci_find_device(void);

#endif /* OWPCI64_H */

