/*
 * owirq64.h - OpenWindows IRQ Management Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWIRQ64_H
#define OWIRQ64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owirq_init(void);
void owirq_register(void);
void owirq_unregister(void);
void owirq_enable(void);
void owirq_disable(void);
void owirq_acknowledge(void);

#endif /* OWIRQ64_H */

