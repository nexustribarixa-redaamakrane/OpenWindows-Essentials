/*
 * owcache64.h - OpenWindows Block Cache Library (.owd)
 *
 * Block cache with least-recently-used eviction. Both the entry table
 * and the block data buffer are caller-provided; the cache only tracks
 * metadata and an LRU clock.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWCACHE64_H
#define OWCACHE64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t block_index;  /* caller-visible block key          */
    bool     valid;        /* slot currently holds a block      */
    bool     dirty;        /* data changed since insert/flush   */
    uint64_t last_used;    /* LRU tick                          */
    uint8_t *data;         /* points into the data buffer       */
} owcache_entry_t;

typedef struct {
    owcache_entry_t *entries;     /* caller-provided entry table */
    uint32_t         capacity;    /* number of entries           */
    uint8_t         *buffer;      /* caller-provided data buffer */
    uint32_t         block_size;  /* bytes per block             */
    uint64_t         clock;       /* monotonic LRU tick          */
    uint64_t         hits;
    uint64_t         misses;
    uint64_t         evictions;
    uint64_t         flushes;
    bool             initialized;
} owcache_t;

/* Bind cache to capacity entries and a buffer holding at least
 * capacity * block_size bytes. All slots start invalid. */
bool owcache_init(owcache_t *cache, owcache_entry_t *entries,
                  uint32_t capacity, uint8_t *buffer, uint32_t block_size);

/* Locate block_index, refreshing its LRU stamp. Returns the slot index
 * or -1 (a miss). */
int64_t owcache_lookup(owcache_t *cache, uint32_t block_index);

/* Store a block, reusing an existing slot or an invalid slot, evicting
 * the least-recently-used entry when the table is full. Returns the
 * slot index or -1. */
int64_t owcache_insert(owcache_t *cache, uint32_t block_index,
                       const void *data, bool dirty);

/* Invalidate the least-recently-used entry. Returns its slot index or
 * -1 when nothing is cached. */
int64_t owcache_evict(owcache_t *cache);

/* Clear the dirty flag on every cached block. Returns the number of
 * entries that were dirty. */
uint32_t owcache_flush(owcache_t *cache);

#endif /* OWCACHE64_H */
