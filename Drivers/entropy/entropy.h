/*
 * entropy.h - OpenWindows Hardware Entropy Source (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ENTROPY_H
#define ENTROPY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void entropy_init(void);
void entropy_read(void);
void entropy_available(void);
void entropy_seed(void);

#endif /* ENTROPY_H */

