/*
 * owslab64.h - OpenWindows Slab Allocator (.owd)
 *
 * Fixed-capacity cache registry. Each cache holds same-sized objects
 * carved from a caller-provided arena and threads a free list through
 * the objects themselves, so object_size must be at least 4.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSLAB64_H
#define OWSLAB64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Upper bound on caches held by one registry. */
#define OWSLAB_MAX_CACHES 16u
#define OWSLAB_NAME_LEN   24u

/* Minimum object size: enough to store the free-list link. */
#define OWSLAB_MIN_OBJECT 4u

typedef struct {
    char      name[OWSLAB_NAME_LEN];
    uint8_t  *slab;         /* caller-provided object arena  */
    uint32_t  object_size;  /* bytes per object (>= 4)       */
    uint32_t  object_count; /* objects in the arena          */
    int32_t   free_head;    /* first free object, -1 if none */
    uint32_t  free_count;   /* objects on the free list      */
    uint32_t  in_use;       /* objects currently allocated   */
    uint32_t  high_water;   /* peak in_use                   */
    bool      active;
} owslab_cache_t;

typedef struct {
    owslab_cache_t caches[OWSLAB_MAX_CACHES];
    uint32_t       cache_count;
    bool           initialized;
} owslab_state_t;

/* Prepare an empty registry. */
bool owslab_init(owslab_state_t *state);

/* Register a cache over arena; returns NULL when the registry is full
 * or the geometry is invalid. */
owslab_cache_t *owslab_create_cache(owslab_state_t *state, const char *name,
                                    void *arena, uint32_t object_size,
                                    uint32_t object_count);

/* Return one object, or NULL when the cache is exhausted. */
void *owslab_alloc(owslab_cache_t *cache);

/* Return an object to its cache. Rejects foreign pointers and double
 * frees. */
bool owslab_free(owslab_cache_t *cache, void *ptr);

/* Retire a cache; outstanding objects should already be freed. */
bool owslab_destroy_cache(owslab_state_t *state, owslab_cache_t *cache);

#endif /* OWSLAB64_H */
