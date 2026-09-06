/*
 * pcspkr.h - OpenWindows PC Speaker Beep Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PCSPKR_H
#define PCSPKR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void pcspkr_init(void);
void pcspkr_beep(void);
void pcspkr_set_freq(void);
void pcspkr_stop(void);

#endif /* PCSPKR_H */

