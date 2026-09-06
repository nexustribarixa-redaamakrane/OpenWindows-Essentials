/*
 * ow_alloc.c — freestanding malloc/realloc/free/calloc over kernel64.
 *
 * Uses k64_request_memory_block (4 KiB page-aligned blocks) as the backing
 * arena. Small allocations are satisfied from 64 KiB slabs carved into
 * uniform power-of-two-ish size classes; allocations larger than one size
 * class get a dedicated k64 block. Every user pointer is 16-byte aligned
 * and carries an 8-byte size header at (user_ptr - 16) so free/realloc are
 * O(1) and require no kernel queries.
 *
 * Compilation profile: freestanding, -nostdlib, -fno-builtin.
 * Single-threaded by construction (CAIRO_NO_MUTEX).
 */
#include "ow_runtime.h"

#include "kernel64/kernel64.h"

/* ------------------------------------------------------------------ */
#define OW_SLAB_SIZE       (65536u)   /* 16 pages per slab              */
#define OW_HDR             (16u)      /* header reservation (16-aligned) */

/* Size classes in bytes (payload after the 16-byte header). */
static const size_t ow_bins[] = {
    16u, 32u, 48u, 64u, 96u, 128u, 192u, 256u,
    384u, 512u, 768u, 1024u, 1536u, 2048u,
    3072u, 4096u, 6144u, 8192u
};
#define OW_NBINS ((int)(sizeof(ow_bins) / sizeof(ow_bins[0])))

static void *ow_free_lists[OW_NBINS];

/* thread-free bump guard: only used to split slabs */
#define OW_PTR_SIZE sizeof(void *)

static size_t ow_class_index(size_t size)
{
    int i;
    if (size > ow_bins[OW_NBINS - 1])
        return (size_t)OW_NBINS; /* large-object path */
    for (i = 0; i < OW_NBINS; ++i)
        if (size <= ow_bins[i])
            return (size_t)i;
    return (size_t)OW_NBINS;
}

static void *ow_request_slab(size_t *out_nchunks, size_t cls)
{
    void *base = 0;
    size_t chunk = ow_bins[cls];
    size_t n = OW_SLAB_SIZE / chunk;
    size_t i;
    k64_status_t st = k64_request_memory_block(OW_SLAB_SIZE, &base);

    if (st != K64_OK || base == 0)
        return 0;

    /* carve: first chunk keeps its own 16-byte header, all are uniform */
    for (i = 0; i < n; ++i) {
        uintptr_t addr = (uintptr_t)base + (i * chunk);
        void *node = (void *)addr;
        *(size_t *)node = chunk;                    /* size header        */
        *(void **)((char *)node + OW_PTR_SIZE) = 0;  /* clear freelink     */
    }
    *out_nchunks = n;
    return base;
}

/* ------------------------------------------------------------------ */

void *malloc(size_t size)
{
    size_t cls, chunk, i, nchunks;
    void *slab, *node;

    if (size == 0)
        return 0;

    cls = ow_class_index(size);
    if (cls == (size_t)OW_NBINS) {
        /* dedicated k64 block: total = align16(size) + header */
        size_t total = (size + OW_HDR + 15u) & ~(size_t)15u;
        void *base = 0;
        k64_status_t st = k64_request_memory_block(total, &base);
        if (st != K64_OK || base == 0)
            return 0;
        *(size_t *)base = total;
        return (void *)((uintptr_t)base + OW_HDR);
    }

    chunk = ow_bins[cls];

    if (ow_free_lists[cls] != 0) {
        node = ow_free_lists[cls];
        ow_free_lists[cls] = *(void **)((char *)node + OW_PTR_SIZE);
        *(size_t *)node = chunk;
        return (void *)((uintptr_t)node + OW_HDR);
    }

    slab = ow_request_slab(&nchunks, cls);
    if (slab == 0)
        return 0;

    /* middle chunks go on the free list; first one is handed out */
    for (i = 1; i < nchunks; ++i) {
        void *cur = (void *)((uintptr_t)slab + (i * chunk));
        *(void **)((char *)cur + OW_PTR_SIZE) = ow_free_lists[cls];
        ow_free_lists[cls] = cur;
    }
    *(size_t *)slab = chunk;
    return (void *)((uintptr_t)slab + OW_HDR);
}

void *calloc(size_t nmemb, size_t size)
{
    size_t total;
    void *p;
    if (nmemb != 0 && size > (size_t)-1 / nmemb)
        return 0;
    total = nmemb * size;
    p = malloc(total);
    if (p)
        memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size)
{
    size_t old;
    void *n;

    if (ptr == 0)
        return malloc(size);
    if (size == 0) {
        free(ptr);
        return 0;
    }

    old = *(size_t *)((uintptr_t)ptr - OW_HDR);
    if (size <= old)
        return ptr;                       /* shrink in place is a no-op */
    n = malloc(size);
    if (n == 0)
        return 0;
    memcpy(n, ptr, old);
    free(ptr);
    return n;
}

void free(void *ptr)
{
    size_t cls, chunk;
    void *head;
    if (ptr == 0)
        return;

    chunk = *(size_t *)((uintptr_t)ptr - OW_HDR);

    if (chunk > ow_bins[OW_NBINS - 1]) {
        void *base = (void *)((uintptr_t)ptr - OW_HDR);
        k64_release_memory_block(base);
        return;
    }

    cls = ow_class_index(chunk);
    head = (void *)((uintptr_t)ptr - OW_HDR);
    *(void **)((char *)head + OW_PTR_SIZE) = ow_free_lists[cls];
    ow_free_lists[cls] = head;
}

/* ------------------------------------------------------------------ */

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    size_t i;
    for (i = 0; i < n; ++i)
        d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d == s || n == 0)
        return dst;
    if (d < s)
        return memcpy(dst, src, n);
    while (n-- > 0)
        d[n] = s[n];
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    unsigned char *d = dst;
    while (n-- > 0)
        *d++ = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *x = a, *y = b;
    while (n-- > 0) {
        if (*x != *y)
            return (int)*x - (int)*y;
        ++x, ++y;
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char *p = s;
    while (n-- > 0) {
        if (*p == (unsigned char)c)
            return (void *)p;
        ++p;
    }
    return 0;
}