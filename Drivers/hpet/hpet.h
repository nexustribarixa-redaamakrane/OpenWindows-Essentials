/*
 * hpet.h - OpenWindows HPET Timer Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef HPET_H
#define HPET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void hpet_init(void);
void hpet_read_counter(void);
void hpet_set_comparator(void);
void hpet_enable(void);

#endif /* HPET_H */

