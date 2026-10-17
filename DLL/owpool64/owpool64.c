/*
 * owpool64.c - OpenWindows Memory Pool Allocator (.owd)
 *
 * Fixed-size blocks linked through an index-based free list. Links are
 * stored with byte-wise access so the arena needs no particular
 * alignment and no strict-aliasing tricks are required.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owpool64.h"

/* ── Free-list link helpers ───────────────────────────────────── */

static void pool_store_next(uint8_t *block, int32_t index)
{
    uint32_t v = (uint32_t)index;
    block[0] = (uint8_t)(v & 0xFFu);
    block[1] = (uint8_t)((v >> 8) & 0xFFu);
    block[2] = (uint8_t)((v >> 16) & 0xFFu);
    block[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static int32_t pool_load_next(const uint8_t *block)
{
    uint32_t v = (uint32_t)block[0]
               | ((uint32_t)block[1] << 8)
               | ((uint32_t)block[2] << 16)
               | ((uint32_t)block[3] << 24);
    return (int32_t)v;
}

static uint8_t *pool_block_at(const owpool_t *pool, uint32_t index)
{
    return pool->arena + (uint64_t)index * (uint64_t)pool->block_size;
}

/* Walk the free list to detect a double free. */
static bool pool_block_is_free(const owpool_t *pool, uint32_t index)
{
    int32_t current = pool->free_head;
    uint32_t guard = 0u;

    while (current >= 0 && guard <= pool->free_count) {
        if ((uint32_t)current == index) return true;
        current = pool_load_next(pool_block_at(pool, (uint32_t)current));
        guard++;
    }
    return false;
}

static void pool_rebuild(owpool_t *pool)
{
    pool->free_head = -1;
    pool->free_count = 0u;
    for (uint32_t i = pool->block_count; i > 0u; --i) {
        uint32_t index = i - 1u;
        uint8_t *block = pool_block_at(pool, index);
        pool_store_next(block, pool->free_head);
        pool->free_head = (int32_t)index;
        pool->free_count++;
    }
}

/* ── Implementation ───────────────────────────────────────────── */

bool owpool_init(owpool_t *pool, void *arena, uint32_t block_size,
                 uint32_t block_count)
{
    if (pool == NULL || arena == NULL) return false;
    if (block_size < OWPOOL_MIN_BLOCK || block_count == 0u) return false;
    if (block_count > 0x7FFFFFFFu) return false;

    pool->arena = (uint8_t *)arena;
    pool->block_size = block_size;
    pool->block_count = block_count;
    pool->in_use = 0u;
    pool->high_water = 0u;
    pool->total_allocs = 0u;
    pool->total_frees = 0u;
    pool->free_head = -1;
    pool->free_count = 0u;

    pool_rebuild(pool);
    pool->initialized = true;
    return true;
}

void *owpool_alloc(owpool_t *pool)
{
    if (pool == NULL || !pool->initialized || pool->free_head < 0) return NULL;

    uint32_t index = (uint32_t)pool->free_head;
    uint8_t *block = pool_block_at(pool, index);

    pool->free_head = pool_load_next(block);
    pool->free_count--;
    pool->in_use++;
    pool->total_allocs++;
    if (pool->in_use > pool->high_water) pool->high_water = pool->in_use;
    return block;
}

bool owpool_free(owpool_t *pool, void *ptr)
{
    if (pool == NULL || !pool->initialized || ptr == NULL) return false;

    uint8_t *block = (uint8_t *)ptr;
    if (block < pool->arena) return false;

    uint64_t offset = (uint64_t)(block - pool->arena);
    if ((offset % pool->block_size) != 0u) return false;

    uint64_t index = offset / pool->block_size;
    if (index >= pool->block_count) return false;
    if (pool_block_is_free(pool, (uint32_t)index)) return false;

    pool_store_next(block, pool->free_head);
    pool->free_head = (int32_t)index;
    pool->free_count++;
    pool->total_frees++;
    if (pool->in_use > 0u) pool->in_use--;
    return true;
}

bool owpool_reset(owpool_t *pool)
{
    if (pool == NULL || !pool->initialized) return false;

    pool_rebuild(pool);
    pool->in_use = 0u;
    pool->high_water = 0u;
    return true;
}

bool owpool_stats(const owpool_t *pool, owpool_stats_t *out)
{
    if (pool == NULL || !pool->initialized || out == NULL) return false;

    out->total_allocs = pool->total_allocs;
    out->total_frees = pool->total_frees;
    out->in_use = pool->in_use;
    out->high_water = pool->high_water;
    out->capacity = pool->block_count;
    return true;
}
