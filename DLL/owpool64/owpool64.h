/*
 * owpool64.h - OpenWindows Memory Pool Allocator (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPOOL64_H
#define OWPOOL64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owpool_init(void);
void owpool_alloc(void);
void owpool_free(void);
void owpool_reset(void);
void owpool_stats(void);

#endif /* OWPOOL64_H */

