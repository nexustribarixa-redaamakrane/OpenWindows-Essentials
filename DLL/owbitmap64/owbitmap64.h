/*
 * owbitmap64.h - OpenWindows Bitmap Allocator (.owd)
 *
 * Simple bit allocator over a caller-provided array of 64-bit words.
 * Used for block maps, IRQ masks and other fixed-size resource sets.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWBITMAP64_H
#define OWBITMAP64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWBMP_BITS_PER_WORD 64u

typedef struct {
    uint64_t *words;        /* caller-provided backing storage     */
    uint32_t  word_count;   /* 64-bit words in the backing array   */
    uint32_t  bit_count;    /* usable bits (<= word_count * 64)    */
    uint32_t  used;         /* number of set bits                  */
    bool      initialized;
} owbitmap_t;

/* Bind bm to words, able to hold bit_count bits. Clears every bit.
 * The array must contain at least (bit_count + 63) / 64 words. */
bool owbmp_init(owbitmap_t *bm, uint64_t *words, uint32_t bit_count);

/* Allocate the first free bit and return its index, or -1. */
int64_t owbmp_alloc(owbitmap_t *bm);

/* Release a previously allocated bit. Returns false when the bit is
 * out of range or already clear. */
bool owbmp_free(owbitmap_t *bm, uint32_t bit);

/* True when the bit is set. Out-of-range bits report false. */
bool owbmp_test(const owbitmap_t *bm, uint32_t bit);

/* Index of the first clear bit, or -1 when the bitmap is full. */
int64_t owbmp_find_first_free(const owbitmap_t *bm);

#endif /* OWBITMAP64_H */
