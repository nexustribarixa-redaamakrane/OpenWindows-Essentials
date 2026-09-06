/*
 * ioapic.h - OpenWindows I/O APIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef IOAPIC_H
#define IOAPIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void ioapic_init(void);
void ioapic_set_irq(void);
void ioapic_mask(void);
void ioapic_unmask(void);
void ioapic_eoi(void);

#endif /* IOAPIC_H */

