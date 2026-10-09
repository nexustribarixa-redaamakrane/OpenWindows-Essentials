/*
 * pcibridge.h - OpenWindows PCI Bus Bridge Enumerator (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PCIBRIDGE_H
#define PCIBRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void pcib_init(void);
void pcib_enumerate(void);
void pcib_read_config(void);
void pcib_write_config(void);

#endif /* PCIBRIDGE_H */

