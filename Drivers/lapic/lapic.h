/*
 * lapic.h - OpenWindows Local APIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef LAPIC_H
#define LAPIC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void lapic_init(void);
void lapic_send_ipi(void);
void lapic_eoi(void);
void lapic_timer_set(void);

#endif /* LAPIC_H */

