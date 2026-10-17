/*
 * owslab64.c - OpenWindows Slab Allocator (.owd)
 *
 * Index-based free lists over caller-owned arenas with a fixed-size
 * cache registry. Object links use byte-wise stores to stay alignment
 * and aliasing clean.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owslab64.h"

/* ── Free-list link helpers ───────────────────────────────────── */

static void slab_store_next(uint8_t *object, int32_t index)
{
    uint32_t v = (uint32_t)index;
    object[0] = (uint8_t)(v & 0xFFu);
    object[1] = (uint8_t)((v >> 8) & 0xFFu);
    object[2] = (uint8_t)((v >> 16) & 0xFFu);
    object[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static int32_t slab_load_next(const uint8_t *object)
{
    uint32_t v = (uint32_t)object[0]
               | ((uint32_t)object[1] << 8)
               | ((uint32_t)object[2] << 16)
               | ((uint32_t)object[3] << 24);
    return (int32_t)v;
}

static uint8_t *slab_object_at(const owslab_cache_t *cache, uint32_t index)
{
    return cache->slab + (uint64_t)index * (uint64_t)cache->object_size;
}

static void slab_copy_name(char *dst, const char *src)
{
    uint32_t i = 0u;
    if (src != NULL) {
        while (src[i] != '\0' && i + 1u < OWSLAB_NAME_LEN) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

/* Walk the free list to detect a double free. */
static bool slab_object_is_free(const owslab_cache_t *cache, uint32_t index)
{
    int32_t current = cache->free_head;
    uint32_t guard = 0u;

    while (current >= 0 && guard <= cache->free_count) {
        if ((uint32_t)current == index) return true;
        current = slab_load_next(slab_object_at(cache, (uint32_t)current));
        guard++;
    }
    return false;
}

static void slab_build_freelist(owslab_cache_t *cache)
{
    cache->free_head = -1;
    cache->free_count = 0u;

    for (uint32_t i = cache->object_count; i > 0u; --i) {
        uint32_t index = i - 1u;
        uint8_t *object = slab_object_at(cache, index);
        slab_store_next(object, cache->free_head);
        cache->free_head = (int32_t)index;
        cache->free_count++;
    }
}

static int32_t slab_cache_index(const owslab_state_t *state,
                                const owslab_cache_t *cache)
{
    for (uint32_t i = 0u; i < OWSLAB_MAX_CACHES; ++i) {
        if (&state->caches[i] == cache) return (int32_t)i;
    }
    return -1;
}

/* ── Implementation ───────────────────────────────────────────── */

bool owslab_init(owslab_state_t *state)
{
    if (state == NULL) return false;

    for (uint32_t i = 0u; i < OWSLAB_MAX_CACHES; ++i) {
        owslab_cache_t *cache = &state->caches[i];
        cache->name[0] = '\0';
        cache->slab = NULL;
        cache->object_size = 0u;
        cache->object_count = 0u;
        cache->free_head = -1;
        cache->free_count = 0u;
        cache->in_use = 0u;
        cache->high_water = 0u;
        cache->active = false;
    }
    state->cache_count = 0u;
    state->initialized = true;
    return true;
}

owslab_cache_t *owslab_create_cache(owslab_state_t *state, const char *name,
                                    void *arena, uint32_t object_size,
                                    uint32_t object_count)
{
    if (state == NULL || !state->initialized || arena == NULL) return NULL;
    if (object_size < OWSLAB_MIN_OBJECT || object_count == 0u) return NULL;
    if (object_count > 0x7FFFFFFFu) return NULL;

    uint32_t slot;
    for (slot = 0u; slot < OWSLAB_MAX_CACHES; ++slot) {
        if (!state->caches[slot].active) break;
    }
    if (slot == OWSLAB_MAX_CACHES) return NULL;

    owslab_cache_t *cache = &state->caches[slot];
    slab_copy_name(cache->name, name);
    cache->slab = (uint8_t *)arena;
    cache->object_size = object_size;
    cache->object_count = object_count;
    cache->in_use = 0u;
    cache->high_water = 0u;

    slab_build_freelist(cache);
    cache->active = true;
    state->cache_count++;
    return cache;
}

void *owslab_alloc(owslab_cache_t *cache)
{
    if (cache == NULL || !cache->active || cache->free_head < 0) return NULL;

    uint32_t index = (uint32_t)cache->free_head;
    uint8_t *object = slab_object_at(cache, index);

    cache->free_head = slab_load_next(object);
    cache->free_count--;
    cache->in_use++;
    if (cache->in_use > cache->high_water) cache->high_water = cache->in_use;
    return object;
}

bool owslab_free(owslab_cache_t *cache, void *ptr)
{
    if (cache == NULL || !cache->active || ptr == NULL) return false;

    uint8_t *object = (uint8_t *)ptr;
    if (object < cache->slab) return false;

    uint64_t offset = (uint64_t)(object - cache->slab);
    if ((offset % cache->object_size) != 0u) return false;

    uint64_t index = offset / cache->object_size;
    if (index >= cache->object_count) return false;
    if (slab_object_is_free(cache, (uint32_t)index)) return false;

    slab_store_next(object, cache->free_head);
    cache->free_head = (int32_t)index;
    cache->free_count++;
    if (cache->in_use > 0u) cache->in_use--;
    return true;
}

bool owslab_destroy_cache(owslab_state_t *state, owslab_cache_t *cache)
{
    if (state == NULL || !state->initialized || cache == NULL) return false;
    if (slab_cache_index(state, cache) < 0) return false;

    cache->active = false;
    cache->name[0] = '\0';
    cache->slab = NULL;
    cache->free_head = -1;
    cache->free_count = 0u;
    cache->in_use = 0u;
    cache->high_water = 0u;
    if (state->cache_count > 0u) state->cache_count--;
    return true;
}
