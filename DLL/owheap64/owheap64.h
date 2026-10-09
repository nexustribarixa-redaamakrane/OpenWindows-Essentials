/*
 * owheap64.h - OpenWindows Managed Heap Allocator (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWHEAP64_H
#define OWHEAP64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owheap_init(void);
void owheap_alloc(void);
void owheap_free(void);
void owheap_realloc(void);
void owheap_stats(void);

#endif /* OWHEAP64_H */

