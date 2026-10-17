/*
 * owcache64.c - OpenWindows Block Cache Library (.owd)
 *
 * Metadata-only cache: block contents live in the caller's buffer and
 * are addressed through per-entry pointers. Lookups bump a monotonic
 * clock used to pick the LRU victim on eviction.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owcache64.h"

/* ── Helpers ──────────────────────────────────────────────────── */

static int64_t cache_find(const owcache_t *cache, uint32_t block_index)
{
    for (uint32_t i = 0u; i < cache->capacity; ++i) {
        if (cache->entries[i].valid &&
            cache->entries[i].block_index == block_index) {
            return (int64_t)i;
        }
    }
    return -1;
}

static int64_t cache_free_slot(const owcache_t *cache)
{
    for (uint32_t i = 0u; i < cache->capacity; ++i) {
        if (!cache->entries[i].valid) return (int64_t)i;
    }
    return -1;
}

static int64_t cache_lru_slot(const owcache_t *cache)
{
    int64_t victim = -1;
    uint64_t oldest = 0u;

    for (uint32_t i = 0u; i < cache->capacity; ++i) {
        if (!cache->entries[i].valid) continue;
        if (victim < 0 || cache->entries[i].last_used < oldest) {
            victim = (int64_t)i;
            oldest = cache->entries[i].last_used;
        }
    }
    return victim;
}

static void cache_copy(uint8_t *dst, const void *src, uint32_t bytes)
{
    const uint8_t *from = (const uint8_t *)src;
    for (uint32_t i = 0u; i < bytes; ++i) dst[i] = from[i];
}

/* ── Implementation ───────────────────────────────────────────── */

bool owcache_init(owcache_t *cache, owcache_entry_t *entries,
                  uint32_t capacity, uint8_t *buffer, uint32_t block_size)
{
    if (cache == NULL || entries == NULL || buffer == NULL) return false;
    if (capacity == 0u || block_size == 0u) return false;

    cache->entries = entries;
    cache->capacity = capacity;
    cache->buffer = buffer;
    cache->block_size = block_size;
    cache->clock = 0u;
    cache->hits = 0u;
    cache->misses = 0u;
    cache->evictions = 0u;
    cache->flushes = 0u;
    cache->initialized = true;

    for (uint32_t i = 0u; i < capacity; ++i) {
        entries[i].block_index = 0u;
        entries[i].valid = false;
        entries[i].dirty = false;
        entries[i].last_used = 0u;
        entries[i].data = buffer + (size_t)i * (size_t)block_size;
    }
    return true;
}

int64_t owcache_lookup(owcache_t *cache, uint32_t block_index)
{
    if (cache == NULL || !cache->initialized) return -1;

    int64_t slot = cache_find(cache, block_index);
    if (slot < 0) {
        cache->misses++;
        return -1;
    }

    cache->clock++;
    cache->entries[(uint32_t)slot].last_used = cache->clock;
    cache->hits++;
    return slot;
}

int64_t owcache_insert(owcache_t *cache, uint32_t block_index,
                       const void *data, bool dirty)
{
    if (cache == NULL || !cache->initialized || data == NULL) return -1;

    int64_t slot = cache_find(cache, block_index);
    if (slot < 0) {
        slot = cache_free_slot(cache);
        if (slot < 0) {
            slot = owcache_evict(cache);
            if (slot < 0) return -1;
        }
    }

    owcache_entry_t *entry = &cache->entries[(uint32_t)slot];
    cache_copy(entry->data, data, cache->block_size);
    entry->block_index = block_index;
    entry->valid = true;
    entry->dirty = dirty;
    cache->clock++;
    entry->last_used = cache->clock;
    return slot;
}

int64_t owcache_evict(owcache_t *cache)
{
    if (cache == NULL || !cache->initialized) return -1;

    int64_t slot = cache_lru_slot(cache);
    if (slot < 0) return -1;

    owcache_entry_t *entry = &cache->entries[(uint32_t)slot];
    entry->valid = false;
    entry->dirty = false;
    entry->last_used = 0u;
    cache->evictions++;
    return slot;
}

uint32_t owcache_flush(owcache_t *cache)
{
    if (cache == NULL || !cache->initialized) return 0u;

    uint32_t flushed = 0u;
    for (uint32_t i = 0u; i < cache->capacity; ++i) {
        if (cache->entries[i].valid && cache->entries[i].dirty) {
            cache->entries[i].dirty = false;
            flushed++;
            cache->flushes++;
        }
    }
    return flushed;
}
