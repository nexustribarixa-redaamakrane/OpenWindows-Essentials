/*
 * owsignal64.h - OpenWindows Signal Handling Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSIGNAL64_H
#define OWSIGNAL64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owsig_init(void);
void owsig_register(void);
void owsig_raise(void);
void owsig_mask(void);
void owsig_pending(void);

#endif /* OWSIGNAL64_H */

