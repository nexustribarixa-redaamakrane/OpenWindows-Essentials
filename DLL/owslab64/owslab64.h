/*
 * owslab64.h - OpenWindows Slab Allocator (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSLAB64_H
#define OWSLAB64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owslab_init(void);
void owslab_alloc(void);
void owslab_free(void);
void owslab_create_cache(void);
void owslab_destroy_cache(void);

#endif /* OWSLAB64_H */

