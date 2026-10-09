/*
 * owcache64.h - OpenWindows Block Cache Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWCACHE64_H
#define OWCACHE64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owcache_init(void);
void owcache_lookup(void);
void owcache_insert(void);
void owcache_evict(void);
void owcache_flush(void);

#endif /* OWCACHE64_H */

