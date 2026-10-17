/*
 * owbitmap64.c - OpenWindows Bitmap Allocator (.owd)
 *
 * Word-at-a-time free-bit search with a tail mask for the final,
 * partially populated word.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owbitmap64.h"

/* ── Helpers ──────────────────────────────────────────────────── */

static uint64_t bmp_mask(uint32_t bit)
{
    return (uint64_t)1u << (bit & (OWBMP_BITS_PER_WORD - 1u));
}

/* ── Implementation ───────────────────────────────────────────── */

bool owbmp_init(owbitmap_t *bm, uint64_t *words, uint32_t bit_count)
{
    if (bm == NULL || words == NULL || bit_count == 0u) return false;
    if (bit_count > (UINT32_MAX - (OWBMP_BITS_PER_WORD - 1u))) return false;

    uint32_t word_count = (bit_count + (OWBMP_BITS_PER_WORD - 1u)) /
                          OWBMP_BITS_PER_WORD;

    for (uint32_t i = 0u; i < word_count; ++i) words[i] = 0u;

    bm->words = words;
    bm->word_count = word_count;
    bm->bit_count = bit_count;
    bm->used = 0u;
    bm->initialized = true;
    return true;
}

int64_t owbmp_find_first_free(const owbitmap_t *bm)
{
    if (bm == NULL || !bm->initialized) return -1;

    for (uint32_t i = 0u; i < bm->word_count; ++i) {
        uint64_t word = bm->words[i];
        if (word == UINT64_MAX) continue;

        uint64_t free_bits = ~word;
        uint32_t base = i * OWBMP_BITS_PER_WORD;

        /* Mask off the unused high bits of the final word. */
        if (base + OWBMP_BITS_PER_WORD > bm->bit_count) {
            uint32_t valid = bm->bit_count - base;
            if (valid < OWBMP_BITS_PER_WORD) {
                free_bits &= (((uint64_t)1u << valid) - 1u);
            }
        }
        if (free_bits == 0u) continue;

        uint32_t bit = 0u;
        while ((free_bits & 1u) == 0u) {
            free_bits >>= 1;
            bit++;
        }
        return (int64_t)(base + bit);
    }
    return -1;
}

int64_t owbmp_alloc(owbitmap_t *bm)
{
    int64_t bit = owbmp_find_first_free(bm);
    if (bit < 0) return -1;

    uint32_t idx = (uint32_t)bit;
    bm->words[idx / OWBMP_BITS_PER_WORD] |= bmp_mask(idx);
    bm->used++;
    return bit;
}

bool owbmp_free(owbitmap_t *bm, uint32_t bit)
{
    if (bm == NULL || !bm->initialized || bit >= bm->bit_count) return false;

    uint32_t idx = bit / OWBMP_BITS_PER_WORD;
    uint64_t mask = bmp_mask(bit);
    if ((bm->words[idx] & mask) == 0u) return false;

    bm->words[idx] &= ~mask;
    if (bm->used > 0u) bm->used--;
    return true;
}

bool owbmp_test(const owbitmap_t *bm, uint32_t bit)
{
    if (bm == NULL || !bm->initialized || bit >= bm->bit_count) return false;
    return (bm->words[bit / OWBMP_BITS_PER_WORD] & bmp_mask(bit)) != 0u;
}
