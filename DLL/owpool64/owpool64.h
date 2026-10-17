/*
 * owpool64.h - OpenWindows Memory Pool Allocator (.owd)
 *
 * Fixed-size block pool. The free list is threaded through the first
 * four bytes of each free block, so block_size must be at least 4.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPOOL64_H
#define OWPOOL64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Minimum block size: enough to store the free-list link. */
#define OWPOOL_MIN_BLOCK 4u

typedef struct {
    uint64_t total_allocs;  /* lifetime allocations          */
    uint64_t total_frees;   /* lifetime frees                */
    uint32_t in_use;        /* blocks currently allocated    */
    uint32_t high_water;    /* peak in_use                   */
    uint32_t capacity;      /* total blocks in the arena     */
} owpool_stats_t;

typedef struct {
    uint8_t  *arena;        /* caller-provided block storage */
    uint32_t  block_size;   /* bytes per block (>= 4)        */
    uint32_t  block_count;  /* number of blocks              */
    int32_t   free_head;    /* first free block, -1 if none  */
    uint32_t  free_count;   /* blocks on the free list       */
    uint32_t  in_use;       /* blocks currently allocated    */
    uint32_t  high_water;   /* peak in_use                   */
    uint64_t  total_allocs;
    uint64_t  total_frees;
    bool      initialized;
} owpool_t;

/* Carve block_count blocks of block_size bytes out of arena. */
bool owpool_init(owpool_t *pool, void *arena, uint32_t block_size,
                 uint32_t block_count);

/* Return one block, or NULL when the pool is exhausted. */
void *owpool_alloc(owpool_t *pool);

/* Return a block to the pool. Rejects pointers outside the arena and
 * blocks that are already free. */
bool owpool_free(owpool_t *pool, void *ptr);

/* Rebuild the free list; every outstanding block is lost. */
bool owpool_reset(owpool_t *pool);

/* Copy the current counters into *out. */
bool owpool_stats(const owpool_t *pool, owpool_stats_t *out);

#endif /* OWPOOL64_H */
