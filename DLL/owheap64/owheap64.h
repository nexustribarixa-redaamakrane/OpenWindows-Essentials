/*
 * owheap64.h - OpenWindows Managed Heap Allocator (.owd)
 *
 * First-fit heap with block headers, block splitting and immediate
 * coalescing of free neighbours. All storage comes from a caller-
 * provided arena; the allocator never calls into libc.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWHEAP64_H
#define OWHEAP64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Payload alignment and header sentinels. */
#define OWHEAP_ALIGN     8u
#define OWHEAP_MAGIC     0x4F57484Fu  /* 'OWHO' */
#define OWHEAP_FLAG_FREE 0x00000001u

/* Precedes every payload; next/prev chain the blocks in address order. */
typedef struct owheap_block {
    uint32_t              size;      /* payload size in bytes        */
    uint32_t              flags;     /* OWHEAP_FLAG_*                */
    uint32_t              magic;     /* OWHEAP_MAGIC when in use     */
    uint32_t              reserved;
    struct owheap_block  *next;      /* next block in address order  */
    struct owheap_block  *prev;      /* previous block               */
} owheap_block_t;

typedef struct {
    uint8_t         *arena;         /* aligned arena base            */
    uint32_t         arena_size;    /* usable bytes in the arena     */
    owheap_block_t  *head;          /* first block                   */
    uint64_t         alloc_count;   /* lifetime allocations          */
    uint64_t         free_count;    /* lifetime frees                */
    uint64_t         bytes_in_use;  /* live payload bytes            */
    uint64_t         peak_bytes;    /* peak bytes_in_use            */
    bool             initialized;
} owheap_t;

typedef struct {
    uint64_t arena_size;
    uint64_t bytes_in_use;
    uint64_t peak_bytes;
    uint64_t alloc_count;
    uint64_t free_count;
    uint32_t block_count;       /* total blocks, free and used   */
    uint32_t free_block_count;  /* free blocks                   */
} owheap_stats_t;

/* Lay a heap over arena. The arena base may be unaligned; the usable
 * region is aligned internally. */
bool owheap_init(owheap_t *heap, void *arena, size_t arena_size);

/* Allocate size bytes, rounded up to OWHEAP_ALIGN. NULL on failure. */
void *owheap_alloc(owheap_t *heap, size_t size);

/* Release a block and coalesce it with free neighbours. */
bool owheap_free(owheap_t *heap, void *ptr);

/* Resize a block, shrinking in place, growing into a free neighbour,
 * or moving to a fresh block. */
void *owheap_realloc(owheap_t *heap, void *ptr, size_t new_size);

/* Copy the heap counters into *out. */
bool owheap_stats(const owheap_t *heap, owheap_stats_t *out);

#endif /* OWHEAP64_H */
