/*
 * owheap64.c - OpenWindows Managed Heap Allocator (.owd)
 *
 * Classic doubly linked first-fit allocator. Blocks are kept in address
 * order, which makes coalescing of physical neighbours a constant-time
 * operation once a block is freed.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owheap64.h"

/* ── Block helpers ────────────────────────────────────────────── */

static uint8_t *heap_payload(owheap_block_t *block)
{
    return (uint8_t *)block + sizeof(owheap_block_t);
}

static owheap_block_t *heap_header(void *ptr)
{
    return (owheap_block_t *)((uint8_t *)ptr - sizeof(owheap_block_t));
}

/* Round a byte count up to OWHEAP_ALIGN, or 0 when it cannot be
 * represented in 32 bits. */
static uint32_t heap_align(size_t size)
{
    size_t aligned = (size + (size_t)(OWHEAP_ALIGN - 1u)) &
                     ~((size_t)OWHEAP_ALIGN - 1u);
    if (aligned < size) return 0u;
    if (aligned > (size_t)0xFFFFFFFFu) return 0u;
    return (uint32_t)aligned;
}

/* Split block so it keeps exactly need bytes. The tail becomes a free
 * block only when it can hold a header plus one aligned unit. */
static void heap_split(owheap_block_t *block, uint32_t need)
{
    uint32_t remain = block->size - need;
    if (remain < (uint32_t)sizeof(owheap_block_t) + OWHEAP_ALIGN) return;

    owheap_block_t *rest = (owheap_block_t *)(heap_payload(block) + need);
    rest->size = remain - (uint32_t)sizeof(owheap_block_t);
    rest->flags = OWHEAP_FLAG_FREE;
    rest->magic = OWHEAP_MAGIC;
    rest->reserved = 0u;
    rest->next = block->next;
    rest->prev = block;
    if (block->next != NULL) block->next->prev = rest;
    block->next = rest;
    block->size = need;
}

/* Merge block with its free physical neighbours. The list is in
 * address order, so next/prev are always adjacent. */
static void heap_coalesce(owheap_block_t *block)
{
    if (block->next != NULL && (block->next->flags & OWHEAP_FLAG_FREE) != 0u) {
        owheap_block_t *next_block = block->next;
        block->size += (uint32_t)sizeof(owheap_block_t) + next_block->size;
        block->next = next_block->next;
        if (next_block->next != NULL) next_block->next->prev = block;
    }
    if (block->prev != NULL && (block->prev->flags & OWHEAP_FLAG_FREE) != 0u) {
        owheap_block_t *prev_block = block->prev;
        prev_block->size += (uint32_t)sizeof(owheap_block_t) + block->size;
        prev_block->next = block->next;
        if (block->next != NULL) block->next->prev = prev_block;
    }
}

static void heap_account_grow(owheap_t *heap, uint32_t amount)
{
    heap->bytes_in_use += amount;
    if (heap->bytes_in_use > heap->peak_bytes) {
        heap->peak_bytes = heap->bytes_in_use;
    }
}

static void heap_account_shrink(owheap_t *heap, uint32_t amount)
{
    if (heap->bytes_in_use >= amount) heap->bytes_in_use -= amount;
    else heap->bytes_in_use = 0u;
}

/* ── Implementation ───────────────────────────────────────────── */

bool owheap_init(owheap_t *heap, void *arena, size_t arena_size)
{
    if (heap == NULL || arena == NULL) return false;

    uintptr_t base = (uintptr_t)arena;
    uintptr_t aligned = (base + (uintptr_t)(OWHEAP_ALIGN - 1u)) &
                        ~((uintptr_t)(OWHEAP_ALIGN - 1u));
    size_t skip = (size_t)(aligned - base);
    if (arena_size <= skip) return false;

    size_t usable = arena_size - skip;
    if (usable <= sizeof(owheap_block_t) + OWHEAP_ALIGN) return false;
    if (usable > (size_t)0xFFFFFFFFu) return false;

    heap->arena = (uint8_t *)aligned;
    heap->arena_size = (uint32_t)usable;
    heap->alloc_count = 0u;
    heap->free_count = 0u;
    heap->bytes_in_use = 0u;
    heap->peak_bytes = 0u;

    owheap_block_t *block = (owheap_block_t *)heap->arena;
    block->size = (uint32_t)usable - (uint32_t)sizeof(owheap_block_t);
    block->flags = OWHEAP_FLAG_FREE;
    block->magic = OWHEAP_MAGIC;
    block->reserved = 0u;
    block->next = NULL;
    block->prev = NULL;
    heap->head = block;

    heap->initialized = true;
    return true;
}

void *owheap_alloc(owheap_t *heap, size_t size)
{
    if (heap == NULL || !heap->initialized || size == 0u) return NULL;

    uint32_t need = heap_align(size);
    if (need == 0u) return NULL;

    for (owheap_block_t *block = heap->head; block != NULL;
         block = block->next) {
        if ((block->flags & OWHEAP_FLAG_FREE) == 0u) continue;
        if (block->size < need) continue;

        heap_split(block, need);
        block->flags &= ~OWHEAP_FLAG_FREE;
        heap->alloc_count++;
        heap_account_grow(heap, block->size);
        return heap_payload(block);
    }
    return NULL;
}

bool owheap_free(owheap_t *heap, void *ptr)
{
    if (heap == NULL || !heap->initialized || ptr == NULL) return false;

    uint8_t *bytes = (uint8_t *)ptr;
    if (bytes < heap->arena || bytes >= heap->arena + heap->arena_size) {
        return false;
    }

    owheap_block_t *block = heap_header(ptr);
    if (block->magic != OWHEAP_MAGIC) return false;
    if ((block->flags & OWHEAP_FLAG_FREE) != 0u) return false;

    heap_account_shrink(heap, block->size);
    block->flags |= OWHEAP_FLAG_FREE;
    heap->free_count++;
    heap_coalesce(block);
    return true;
}

void *owheap_realloc(owheap_t *heap, void *ptr, size_t new_size)
{
    if (heap == NULL || !heap->initialized) return NULL;
    if (ptr == NULL) return owheap_alloc(heap, new_size);
    if (new_size == 0u) {
        owheap_free(heap, ptr);
        return NULL;
    }

    uint8_t *bytes = (uint8_t *)ptr;
    if (bytes < heap->arena || bytes >= heap->arena + heap->arena_size) {
        return NULL;
    }

    owheap_block_t *block = heap_header(ptr);
    if (block->magic != OWHEAP_MAGIC) return NULL;
    if ((block->flags & OWHEAP_FLAG_FREE) != 0u) return NULL;

    uint32_t need = heap_align(new_size);
    if (need == 0u) return NULL;

    uint32_t original = block->size;
    heap_account_shrink(heap, original);

    /* Shrink in place, handing any worthwhile remainder back. */
    if (need <= original) {
        heap_split(block, need);
        heap_account_grow(heap, block->size);
        return ptr;
    }

    /* Grow into a free successor when it supplies enough room. */
    owheap_block_t *next_block = block->next;
    if (next_block != NULL && (next_block->flags & OWHEAP_FLAG_FREE) != 0u &&
        original + (uint32_t)sizeof(owheap_block_t) + next_block->size >= need) {
        block->size = original + (uint32_t)sizeof(owheap_block_t) +
                      next_block->size;
        block->next = next_block->next;
        if (next_block->next != NULL) next_block->next->prev = block;
        heap_split(block, need);
        heap_account_grow(heap, block->size);
        return ptr;
    }

    /* Restore accounting before handing the block back to the heap. */
    heap_account_grow(heap, original);

    void *fresh = owheap_alloc(heap, new_size);
    if (fresh == NULL) return NULL;

    uint32_t copy = (original < need) ? original : need;
    uint8_t *dst = (uint8_t *)fresh;
    for (uint32_t i = 0u; i < copy; ++i) dst[i] = bytes[i];
    owheap_free(heap, ptr);
    return fresh;
}

bool owheap_stats(const owheap_t *heap, owheap_stats_t *out)
{
    if (heap == NULL || !heap->initialized || out == NULL) return false;

    uint32_t blocks = 0u;
    uint32_t free_blocks = 0u;
    for (const owheap_block_t *block = heap->head; block != NULL;
         block = block->next) {
        blocks++;
        if ((block->flags & OWHEAP_FLAG_FREE) != 0u) free_blocks++;
    }

    out->arena_size = heap->arena_size;
    out->bytes_in_use = heap->bytes_in_use;
    out->peak_bytes = heap->peak_bytes;
    out->alloc_count = heap->alloc_count;
    out->free_count = heap->free_count;
    out->block_count = blocks;
    out->free_block_count = free_blocks;
    return true;
}
