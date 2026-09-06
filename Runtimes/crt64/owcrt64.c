/*
 * owcrt64.c - owcrt64.owd implementation (Core C/C++ Runtime Environment)
 *
 * Freestanding core runtime for OpenWindows modules:
 *   - process startup/exit + atexit + __cxa_* support stubs
 *   - boundary-tag segmented heap over kernel64.owd page descriptors
 *     (16-byte headers, tail canaries, guard pages, corruption counters)
 *   - classic mem/str primitives reimplemented freestanding
 *   - a minimal full-featured integer/pointer formatter (ow_snprintf)
 *   - TLS block registry keyed on native thread handles
 *
 * C99 freestanding strict profile; the only external symbols are the
 * kernel64.owd API (late-bound at module load) and libgcc helpers.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include "owcrt64.h"            /* Extensions ABI mirror */
#include "owd_format.h"
#include "owc_format.h"
#include "kernel64.h"

/* OWD1 binary header metadata (documentary; see Extensions/owd_format.h). */
#define OWCRT64_LIB_NAME        "owcrt64.owd"
#define OWCRT64_LIB_TYPE        OWD_LIBTYPE_HYBRID
#define OWCRT64_TARGET_ARCH     0x02u
#define OWCRT64_ALIGNMENT_LOG2  4u
#define OWCRT64_INIT_FLAGS      (OWC_INIT_REQUIRES_OWRP | \
                                 OWC_INIT_REQUIRES_BANC)

#define OWCRT64_LIB_VERSION     "owcrt64.owd 1.0.0"

static const uint8_t owcrt_ident[] = OWCRT64_LIB_VERSION;

/* ------------------------------------------------------------------ */
/*  Module lifecycle                                                   */
/* ------------------------------------------------------------------ */

static bool owcrt_initialized = false;

owcrt64_status_t owcrt64_module_init(void)
{
    if (owcrt_initialized) {
        return OWCRT64_OK;
    }
    if (k64_initialize_api() != K64_OK) {
        return OWCRT64_BAN_K64_BOOT;
    }
    owcrt_initialized = true;
    return OWCRT64_OK;
}

owcrt64_status_t owcrt64_module_shutdown(void)
{
    if (!owcrt_initialized) {
        return OWCRT64_ERR_UNINITIALIZED;
    }
    owcrt_initialized = false;
    return OWCRT64_OK;
}

uint32_t owcrt64_abi_version(void)
{
    return (uint32_t)((1u * 10000u) + (0u * 100u) + 0u);
}

uint16_t owcrt64_abi_major(void) { return 1u; }
uint16_t owcrt64_abi_minor(void) { return 0u; }
const uint8_t *owcrt64_ident(void) { return owcrt_ident; }

/* ------------------------------------------------------------------ */
/*  Startup / exit registries                                          */
/* ------------------------------------------------------------------ */

#define OWCRT_ATEXIT_MAX    64u

typedef struct {
    void (*fn)(void *);
    void  *arg;
    void  *dso;
    bool   used;
} owcrt_exit_entry_t;

static owcrt_exit_entry_t owcrt_exit_tab[OWCRT_ATEXIT_MAX];

int __cxa_atexit(void (*fn)(void *), void *arg, void *dso)
{
    if (fn == NULL) {
        return 0;
    }
    for (uint32_t i = 0u; i < OWCRT_ATEXIT_MAX; i++) {
        if (!owcrt_exit_tab[i].used) {
            owcrt_exit_tab[i].fn   = fn;
            owcrt_exit_tab[i].arg  = arg;
            owcrt_exit_tab[i].dso  = dso;
            owcrt_exit_tab[i].used = true;
            return 0;
        }
    }
    return 1;
}

void __cxa_finalize(void *dso)
{
    /* Run registrations in reverse registration order. */
    for (uint32_t i = OWCRT_ATEXIT_MAX; i-- > 0u;) {
        if (!owcrt_exit_tab[i].used) {
            continue;
        }
        if (dso != NULL && owcrt_exit_tab[i].dso != dso) {
            continue;
        }
        void (*fn)(void *) = owcrt_exit_tab[i].fn;
        void *arg          = owcrt_exit_tab[i].arg;
        owcrt_exit_tab[i].used = false;
        fn(arg);
    }
}

void __cxa_pure_virtual(void)
{
    (void)k64_raise_system_exception(OWCRT64_BAN_PURE_VIRTUAL, 0u);
}

owcrt64_status_t owcrt64_atexit(void (*fn)(void))
{
    if (__cxa_atexit((void (*)(void *))fn, NULL, NULL) != 0) {
        return OWCRT64_ERR_NO_ATEXIT;
    }
    return OWCRT64_OK;
}

owcrt64_status_t owcrt64_start(void)
{
    owcrt64_status_t st = owcrt64_module_init();
    if (st != OWCRT64_OK) {
        return st;
    }
    return OWCRT64_OK;
}

owcrt64_status_t owcrt64_exit(int32_t code)
{
    __cxa_finalize(NULL);
    return (code == 0) ? OWCRT64_OK : OWCRT64_ERR_BAD_PARAM;
}

/* ------------------------------------------------------------------ */
/*  Heap: boundary-tag segmented allocator over kernel64.owd           */
/* ------------------------------------------------------------------ */

/*  Block layout (all sizes in bytes):
 *
 *      [ hdr 16 ] = { u32 size; u32 flags; u32 magic; u32 gap; }
 *      [ payload ]      size - 28 bytes of caller memory
 *      [ canary 4 ]     OWCRT64_CANARY | caller-size-bits
 *      [ ftar 8 ]  = { u32 size; u32 flags; }
 *
 *  Free blocks overwrite the first 16 bytes of the payload region with
 *  next/prev list pointers. size in hdr/footer spans the WHOLE block
 *  including header+canary+footer. flags bit0 = used.
 *
 *  Large allocations (>= OWCRT64_LARGE_THRESHOLD) get a dedicated kernel
 *  segment with the guard-page flag; their header carries segment index.
 */

#define OWCRT_SEG_MAX        16u
#define OWCRT_SEG_HDR        32u
#define OWCRT_BLK_HDR        16u
#define OWCRT_BLK_CNARY      4u
#define OWCRT_BLK_FTAR       8u

#define OWCRT_MAGIC_BLK      0x4F574231u      /* "OWB1"                 */
#define OWCRT_MAGIC_SEG      0x4F574353u      /* "OWCS"                 */
#define OWCRT_MAGIC_END      0x4F57454Eu      /* "OWEN" arena terminator*/
#define OWCRT_FLAG_USED      0x00000001u
#define OWCRT_FLAG_LARGE     0x00000002u

/*  Free-list pointers live in the first 16 bytes of a freed block's
 *  payload region, so a free block needs a payload >= 16 bytes
 *  (total = HDR+payload+CNARY+FTAR >= 44). */
#define OWCRT_MIN_FREE_USER  16u
#define OWCRT_MIN_FREE_TOTAL 44u
#define OWCRT_END_BLOCK_SIZE 24u   /* HDR + FTAR, always "used"       */

typedef struct {
    uint8_t *base;
    size_t   size;
    uint32_t flags;
    bool     active;
} owcrt_seg_t;

static owcrt_seg_t owcrt_segs[OWCRT_SEG_MAX];

static uint64_t owcrt_bytes_allocated;
static uint64_t owcrt_bytes_segmented;
static uint64_t owcrt_allocs;
static uint64_t owcrt_frees;
static uint64_t owcrt_guard_faults;

static uint8_t *owcrt_free_head;   /* head of small-block free list   */
static bool owcrt_heap_corrupt;

void *ow_memset(void *dst, int c, size_t n);   /* defined below */

static void owcrt_store_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static uint32_t owcrt_load_u32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void owcrt_store_u64(uint8_t *p, uint64_t v)
{
    for (uint32_t i = 0u; i < 8u; i++) {
        p[i] = (uint8_t)((v >> (8u * i)) & 0xFFu);
    }
}

static uint64_t owcrt_load_u64(const uint8_t *p)
{
    uint64_t v = 0u;
    for (uint32_t i = 0u; i < 8u; i++) {
        v |= ((uint64_t)p[i]) << (8u * i);
    }
    return v;
}

/* ---------- block helpers (small arena blocks) ---------- */

static size_t owcrt_blk_user(const uint8_t *blk)
{
    return owcrt_load_u32(blk) - OWCRT_BLK_HDR - OWCRT_BLK_CNARY - OWCRT_BLK_FTAR;
}

static void owcrt_blk_stamp(const uint8_t *blk, size_t total, uint32_t flags)
{
    owcrt_store_u32((uint8_t *)blk, (uint32_t)total);
    owcrt_store_u32((uint8_t *)blk + 4u, flags);
    owcrt_store_u32((uint8_t *)blk + 8u, OWCRT_MAGIC_BLK);
    owcrt_store_u32((uint8_t *)blk + 12u, 0u);
    /* canary = OWCRT64_CANARY | (low bits of user size so free can check) */
    owcrt_store_u32((uint8_t *)blk + OWCRT_BLK_HDR + owcrt_blk_user(blk),
                    OWCRT64_CANARY | (uint32_t)(owcrt_blk_user(blk) & 0xFFu));
    owcrt_store_u32((uint8_t *)blk + total - OWCRT_BLK_FTAR, (uint32_t)total);
    owcrt_store_u32((uint8_t *)blk + total - OWCRT_BLK_FTAR + 4u, flags);
}

static void owcrt_blk_free_link(uint8_t *blk)
{
    owcrt_store_u64(blk + OWCRT_BLK_HDR, (uint64_t)(uintptr_t)owcrt_free_head);
    owcrt_store_u64(blk + OWCRT_BLK_HDR + 8u, 0u);
    if (owcrt_free_head != NULL) {
        owcrt_store_u64(owcrt_free_head + OWCRT_BLK_HDR + 8u,
                        (uint64_t)(uintptr_t)blk);
    }
    owcrt_free_head = blk;
}

static void owcrt_blk_free_unlink(uint8_t *blk)
{
    uint64_t prev = owcrt_load_u64(blk + OWCRT_BLK_HDR + 8u);
    uint64_t next = owcrt_load_u64(blk + OWCRT_BLK_HDR);
    if (prev != 0u) {
        owcrt_store_u64((uint8_t *)(uintptr_t)prev + OWCRT_BLK_HDR,
                        next);
    } else {
        owcrt_free_head = (uint8_t *)(uintptr_t)next;
    }
    if (next != 0u) {
        owcrt_store_u64((uint8_t *)(uintptr_t)next + OWCRT_BLK_HDR + 8u,
                        prev);
    }
}

/* ---------- segment management ---------- */

static owcrt64_status_t owcrt_seg_new(size_t bytes, owcrt_seg_t **out)
{
    void *p = NULL;
    if (bytes < OWCRT64_SEGMENT_MIN) {
        bytes = OWCRT64_SEGMENT_MIN;
    }
    if (k64_request_memory_block(bytes, &p) != K64_OK || p == NULL) {
        return OWCRT64_BAN_OUT_OF_MEMORY;
    }
    owcrt_seg_t *sel = NULL;
    for (uint32_t i = 0u; i < OWCRT_SEG_MAX; i++) {
        if (!owcrt_segs[i].active) {
            sel = &owcrt_segs[i];
            break;
        }
    }
    if (sel == NULL) {
        (void)k64_release_memory_block(p);
        return OWCRT64_BAN_OUT_OF_MEMORY;
    }
    owcrt_store_u32((uint8_t *)p, OWCRT_MAGIC_SEG);
    owcrt_store_u32((uint8_t *)p + 4u, (uint32_t)bytes);
    owcrt_store_u32((uint8_t *)p + 8u, (uint32_t)(sel - owcrt_segs));
    owcrt_store_u32((uint8_t *)p + 12u, 0u);
    sel->base   = (uint8_t *)p;
    sel->size   = bytes;
    sel->flags  = K64_MEM_FLAG_GUARD;
    sel->active = true;
    owcrt_bytes_segmented += bytes;
    *out = sel;
    return OWCRT64_OK;
}

static void owcrt_seg_destroy(owcrt_seg_t *seg)
{
    owcrt_bytes_segmented -= seg->size;
    (void)k64_release_memory_block(seg->base);
    ow_memset(seg, 0, sizeof(*seg));
}

/* Split `blk` (total size S, free) keeping `user` bytes then a remainder
 * block that can itself hold free-list pointer state (>= OWCRT_MIN_FREE_TOTAL). */
static bool owcrt_blk_split(uint8_t *blk, size_t user)
{
    size_t take = OWCRT_BLK_HDR + user + OWCRT_BLK_CNARY + OWCRT_BLK_FTAR;
    size_t total = owcrt_load_u32(blk);
    if (total < take + OWCRT_MIN_FREE_TOTAL) {
        return false;
    }
    uint8_t *rest = blk + take;
    owcrt_blk_stamp(rest, total - take, 0u);
    owcrt_blk_free_link(rest);
    owcrt_blk_stamp(blk, take, 0u);
    return true;
}

static void owcrt_blk_free_core(uint8_t *blk)
{
    size_t total = owcrt_load_u32(blk);

    /* Coalesce with previous block if free (read its footer). */
    uint8_t *prev_foot = blk - OWCRT_BLK_FTAR;
    if (prev_foot >= (uint8_t *)(uintptr_t)0x100u) { /* sane lower bound */
        uint32_t psz  = owcrt_load_u32(prev_foot);
        uint32_t pflg = owcrt_load_u32(prev_foot + 4u);
        if ((pflg & OWCRT_FLAG_USED) == 0u && (pflg & 0xFFFFFFFEu) == 0u) {
            uint8_t *prev = prev_foot - (psz - OWCRT_BLK_FTAR);
            if (psz >= OWCRT_MIN_FREE_TOTAL && prev + psz == blk) {
                owcrt_blk_free_unlink(prev);
                blk   = prev;
                total = total + psz;
            }
        }
    }
    /* Coalesce with the next block if it is a free block (never the END
     * marker: it stays "used", and it bounds the read). */
    uint8_t *next = blk + total;
    uint32_t nsz  = owcrt_load_u32(next);
    uint32_t nflg = owcrt_load_u32(next + 4u);
    if ((nflg & OWCRT_FLAG_USED) == 0u && (nflg & 0xFFFFFFFEu) == 0u
        && nsz >= OWCRT_MIN_FREE_TOTAL) {
        owcrt_blk_free_unlink(next);
        total = total + nsz;
    }
    owcrt_blk_stamp(blk, total, 0u);
    owcrt_blk_free_link(blk);
}

void *ow_malloc(size_t size)
{
    if (!owcrt_initialized) {
        (void)owcrt64_module_init();
    }
    if (owcrt_heap_corrupt) {
        return NULL;
    }
    size_t user = size;
    if (size > SIZE_MAX - 32u) {
        return NULL;
    }

    if (user >= OWCRT64_LARGE_THRESHOLD) {
        size_t segbytes = user + OWCRT_BLK_HDR + OWCRT_BLK_CNARY;
        segbytes = (segbytes + 0xFFFu) & ~(size_t)0xFFFu;
        owcrt_seg_t *seg = NULL;
        if (owcrt_seg_new(segbytes, &seg) != OWCRT64_OK) {
            return NULL;
        }
        owcrt_store_u32(seg->base, (uint32_t)user);
        owcrt_store_u32(seg->base + 4u, OWCRT_FLAG_LARGE);
        owcrt_store_u32(seg->base + 8u, OWCRT_MAGIC_BLK);
        owcrt_store_u32(seg->base + 12u, (uint32_t)(seg - owcrt_segs));
        owcrt_store_u32(seg->base + OWCRT_BLK_HDR + user,
                        OWCRT64_CANARY | (uint32_t)(user & 0xFFu));
        owcrt_bytes_allocated += user;
        owcrt_allocs++;
        return seg->base + OWCRT_BLK_HDR;
    }

    if (user < OWCRT_MIN_FREE_USER) {
        user = OWCRT_MIN_FREE_USER;   /* keep freed blocks list-safe */
    }

    /* Small allocation: first-fit over the free list. */
    uint8_t *it = owcrt_free_head;
    while (it != NULL) {
        uint64_t nxt = owcrt_load_u64(it + OWCRT_BLK_HDR);
        uint32_t flg = owcrt_load_u32(it + 4u);
        uint32_t sz  = owcrt_load_u32(it);
        if ((flg & OWCRT_FLAG_USED) == 0u
            && sz >= OWCRT_BLK_HDR + user + OWCRT_BLK_CNARY + OWCRT_BLK_FTAR) {
            owcrt_blk_free_unlink(it);
            (void)owcrt_blk_split(it, user);
            size_t take = OWCRT_BLK_HDR + user + OWCRT_BLK_CNARY + OWCRT_BLK_FTAR;
            size_t real = owcrt_load_u32(it);
            owcrt_blk_stamp(it, take < real ? real : take, OWCRT_FLAG_USED);
            owcrt_bytes_allocated += user;
            owcrt_allocs++;
            return it + OWCRT_BLK_HDR;
        }
        it = (uint8_t *)(uintptr_t)nxt;
    }

    /* No fit: carve a fresh segment with an END marker bounding the arena,
     * then take a fresh block from the front of the usable region. */
    owcrt_seg_t *seg = NULL;
    if (owcrt_seg_new(OWCRT64_SEGMENT_DEFAULT, &seg) != OWCRT64_OK) {
        return NULL;
    }
    size_t usable = seg->size - OWCRT_SEG_HDR - OWCRT_END_BLOCK_SIZE;
    owcrt_store_u32(seg->base + OWCRT_SEG_HDR + usable, OWCRT_MAGIC_END);
    owcrt_store_u32(seg->base + OWCRT_SEG_HDR + usable + 4u, OWCRT_FLAG_USED);
    owcrt_store_u32(seg->base + OWCRT_SEG_HDR + usable + 8u, OWCRT_MAGIC_END);
    owcrt_store_u32(seg->base + OWCRT_SEG_HDR + usable + 12u, OWCRT_FLAG_USED);
    owcrt_store_u32(seg->base + OWCRT_SEG_HDR + usable + 16u,
                    OWCRT_END_BLOCK_SIZE);
    owcrt_store_u32(seg->base + OWCRT_SEG_HDR + usable + 20u, OWCRT_FLAG_USED);
    uint8_t *blk = seg->base + OWCRT_SEG_HDR;
    size_t   total = usable;
    owcrt_blk_stamp(blk, total, 0u);
    (void)owcrt_blk_split(blk, user);
    owcrt_blk_stamp(blk, owcrt_load_u32(blk), OWCRT_FLAG_USED);
    owcrt_bytes_allocated += user;
    owcrt_allocs++;
    return blk + OWCRT_BLK_HDR;
}

void *ow_calloc(size_t nmemb, size_t size)
{
    if (nmemb != 0u && size > SIZE_MAX / nmemb) {
        return NULL;
    }
    size_t total = nmemb * size;
    void *p = ow_malloc(total);
    if (p != NULL) {
        ow_memset(p, 0, total);
    }
    return p;
}

void ow_free(void *ptr)
{
    if (ptr == NULL) {
        return;
    }
    uint8_t *blk = (uint8_t *)ptr - OWCRT_BLK_HDR;
    uint32_t magic = owcrt_load_u32(blk + 8u);
    if (magic != OWCRT_MAGIC_BLK) {
        owcrt_guard_faults++;
        owcrt_heap_corrupt = true;
        return;
    }
    uint32_t flags = owcrt_load_u32(blk + 4u);
    uint32_t usz   = owcrt_load_u32(blk);
    if ((flags & OWCRT_FLAG_LARGE) != 0u) {
        uint32_t idx = owcrt_load_u32(blk + 12u);
        uint32_t expected = OWCRT64_CANARY | (usz & 0xFFu);
        if (idx >= OWCRT_SEG_MAX
            || owcrt_load_u32(blk + OWCRT_BLK_HDR + usz) != expected) {
            owcrt_guard_faults++;
            owcrt_heap_corrupt = true;
            return;
        }
        owcrt_bytes_allocated -= usz;
        owcrt_frees++;
        owcrt_seg_destroy(&owcrt_segs[idx]);
        return;
    }
    if ((flags & 0xFFFFFFFEu) != 0u
        || usz < OWCRT_MIN_FREE_TOTAL) {
        owcrt_guard_faults++;
        owcrt_heap_corrupt = true;
        return;
    }
    uint32_t expected = OWCRT64_CANARY | ((uint32_t)owcrt_blk_user(blk) & 0xFFu);
    if (owcrt_load_u32(blk + OWCRT_BLK_HDR + owcrt_blk_user(blk)) != expected) {
        owcrt_guard_faults++;
        owcrt_heap_corrupt = true;
        return;
    }
    owcrt_bytes_allocated -= owcrt_blk_user(blk);
    owcrt_frees++;
    owcrt_blk_free_core(blk);
}

void *ow_realloc(void *ptr, size_t size)
{
    if (ptr == NULL) {
        return ow_malloc(size);
    }
    if (size == 0u) {
        ow_free(ptr);
        return NULL;
    }
    uint8_t *blk = (uint8_t *)ptr - OWCRT_BLK_HDR;
    uint32_t magic = owcrt_load_u32(blk + 8u);
    uint32_t flags = owcrt_load_u32(blk + 4u);
    if (magic != OWCRT_MAGIC_BLK
        || (flags & 0xFFFFFFFEu) != 0u) {
        owcrt_guard_faults++;
        owcrt_heap_corrupt = true;
        return NULL;
    }
    size_t old = owcrt_blk_user(blk);
    if (size <= old) {
        return ptr;
    }
    void *np = ow_malloc(size);
    if (np == NULL) {
        return NULL;
    }
    ow_memcpy(np, ptr, old);
    ow_free(ptr);
    return np;
}

size_t ow_malloc_usable_size(const void *ptr)
{
    if (ptr == NULL) {
        return 0u;
    }
    const uint8_t *blk = (const uint8_t *)ptr - OWCRT_BLK_HDR;
    if (owcrt_load_u32(blk + 8u) != OWCRT_MAGIC_BLK) {
        return 0u;
    }
    return owcrt_blk_user(blk);
}

owcrt64_status_t owcrt64_heap_stats(owcrt64_heap_stats_t *out)
{
    if (out == NULL) {
        return OWCRT64_ERR_BAD_PARAM;
    }
    out->bytes_allocated = owcrt_bytes_allocated;
    out->bytes_segmented = owcrt_bytes_segmented;
    out->allocs          = owcrt_allocs;
    out->frees           = owcrt_frees;
    out->guard_faults    = owcrt_guard_faults;
    out->segments        = 0u;
    for (uint32_t i = 0u; i < OWCRT_SEG_MAX; i++) {
        if (owcrt_segs[i].active) {
            out->segments++;
        }
    }
    return OWCRT64_OK;
}

/* ------------------------------------------------------------------ */
/*  TLS block registry                                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    size_t size;
    bool   used;
} owcrt_tls_slot_t;

static owcrt_tls_slot_t owcrt_tls_slots[OWCRT64_TLS_SLOTS];
static void *owcrt_tls_blocks[OWCRT64_TLS_THREADS][OWCRT64_TLS_SLOTS];
static uintptr_t owcrt_tls_threads[OWCRT64_TLS_THREADS];
static uint32_t owcrt_tls_thread_count;

static int32_t owcrt_tls_find_thread(uintptr_t thread)
{
    for (uint32_t i = 0u; i < owcrt_tls_thread_count; i++) {
        if (owcrt_tls_threads[i] == thread) {
            return (int32_t)i;
        }
    }
    return -1;
}

owcrt64_status_t owcrt64_tls_slot_create(size_t size, uint32_t *out_slot)
{
    if (out_slot == NULL) {
        return OWCRT64_ERR_BAD_PARAM;
    }
    for (uint32_t i = 0u; i < OWCRT64_TLS_SLOTS; i++) {
        if (!owcrt_tls_slots[i].used) {
            owcrt_tls_slots[i].used = true;
            owcrt_tls_slots[i].size = size;
            *out_slot = i;
            return OWCRT64_OK;
        }
    }
    return OWCRT64_ERR_NO_TLS_SLOT;
}

owcrt64_status_t owcrt64_tls_slot_destroy(uint32_t slot)
{
    if (slot >= OWCRT64_TLS_SLOTS || !owcrt_tls_slots[slot].used) {
        return OWCRT64_ERR_BAD_PARAM;
    }
    for (uint32_t t = 0u; t < owcrt_tls_thread_count; t++) {
        if (owcrt_tls_blocks[t][slot] != NULL) {
            ow_free(owcrt_tls_blocks[t][slot]);
            owcrt_tls_blocks[t][slot] = NULL;
        }
    }
    owcrt_tls_slots[slot].used = false;
    owcrt_tls_slots[slot].size = 0u;
    return OWCRT64_OK;
}

void *owcrt64_tls_get(uint32_t slot, uintptr_t thread)
{
    if (slot >= OWCRT64_TLS_SLOTS || !owcrt_tls_slots[slot].used) {
        return NULL;
    }
    int32_t t = owcrt_tls_find_thread(thread);
    if (t < 0) {
        if (owcrt_tls_thread_count >= OWCRT64_TLS_THREADS) {
            return NULL;
        }
        owcrt_tls_threads[owcrt_tls_thread_count] = thread;
        t = (int32_t)owcrt_tls_thread_count;
        owcrt_tls_thread_count++;
    }
    if (owcrt_tls_blocks[t][slot] == NULL) {
        owcrt_tls_blocks[t][slot] = ow_malloc(owcrt_tls_slots[slot].size);
        if (owcrt_tls_blocks[t][slot] != NULL) {
            ow_memset(owcrt_tls_blocks[t][slot], 0, owcrt_tls_slots[slot].size);
        }
    }
    return owcrt_tls_blocks[t][slot];
}

owcrt64_status_t owcrt64_tls_thread_exit(uintptr_t thread)
{
    int32_t t = owcrt_tls_find_thread(thread);
    if (t < 0) {
        return OWCRT64_OK;
    }
    for (uint32_t s = 0u; s < OWCRT64_TLS_SLOTS; s++) {
        if (owcrt_tls_blocks[t][s] != NULL) {
            ow_free(owcrt_tls_blocks[t][s]);
            owcrt_tls_blocks[t][s] = NULL;
        }
    }
    if ((uint32_t)t + 1u < owcrt_tls_thread_count) {
        owcrt_tls_threads[t] = owcrt_tls_threads[owcrt_tls_thread_count - 1u];
        for (uint32_t s = 0u; s < OWCRT64_TLS_SLOTS; s++) {
            owcrt_tls_blocks[t][s] = owcrt_tls_blocks[owcrt_tls_thread_count - 1u][s];
        }
    }
    owcrt_tls_thread_count--;
    return OWCRT64_OK;
}

/* ------------------------------------------------------------------ */
/*  Memory primitives                                                  */
/* ------------------------------------------------------------------ */

void *ow_memcpy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    while (n >= 8u) {
        owcrt_store_u64(d, owcrt_load_u64(s));
        d += 8u; s += 8u; n -= 8u;
    }
    while (n-- > 0u) {
        *d++ = *s++;
    }
    return dst;
}

void *ow_memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    uint64_t v = (uint8_t)c;
    v |= v << 8; v |= v << 16; v |= v << 32;
    while (n >= 8u) {
        owcrt_store_u64(d, v);
        d += 8u; n -= 8u;
    }
    while (n-- > 0u) {
        *d++ = (uint8_t)c;
    }
    return dst;
}

void *ow_memmove(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        while (n >= 8u) {
            owcrt_store_u64(d, owcrt_load_u64(s));
            d += 8u; s += 8u; n -= 8u;
        }
        while (n-- > 0u) {
            *d++ = *s++;
        }
    } else if (d > s) {
        d += n; s += n;
        while (n >= 8u) {
            d -= 8u; s -= 8u;
            owcrt_store_u64(d, owcrt_load_u64(s));
            n -= 8u;
        }
        while (n-- > 0u) {
            *--d = *--s;
        }
    }
    return dst;
}

int ow_memcmp(const void *a, const void *b, size_t n)
{
    const uint8_t *x = (const uint8_t *)a;
    const uint8_t *y = (const uint8_t *)b;
    for (size_t i = 0u; i < n; i++) {
        if (x[i] != y[i]) {
            return (x[i] < y[i]) ? -1 : 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/*  String primitives                                                  */
/* ------------------------------------------------------------------ */

size_t ow_strlen(const char *s)
{
    size_t n = 0u;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

int ow_strcmp(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return (uint8_t)*a - (uint8_t)*b;
}

int ow_strncmp(const char *a, const char *b, size_t n)
{
    while (n-- > 0u) {
        if (*a != *b) {
            return (uint8_t)*a - (uint8_t)*b;
        }
        if (*a == '\0') {
            return 0;
        }
        a++;
        b++;
    }
    return 0;
}

char *ow_strcpy(char *dst, const char *src)
{
    char *d = dst;
    while (*src != '\0') {
        *d++ = *src++;
    }
    *d = '\0';
    return dst;
}

char *ow_strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0u;
    for (; i < n && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    for (; i < n; i++) {
        dst[i] = '\0';
    }
    return dst;
}

/* ------------------------------------------------------------------ */
/*  ow_snprintf - freestanding formatter                               */
/* ------------------------------------------------------------------ */

static void owcrt_fmt_put(char *out, size_t cap, size_t *wp, char c)
{
    if (*wp < cap) {
        out[*wp] = c;
    }
    (*wp)++;
}

static void owcrt_fmt_reverse(char *b, size_t n)
{
    for (size_t i = 0u, j = n; i < j--; i++) {
        char t  = b[i];
        b[i] = b[j];
        b[j] = t;
    }
}

/*
 * Emit an unsigned value with the requested radix + case. Returns chars
 * written. Type widths: v is a uint64_t already extended by the caller.
 */
static void owcrt_fmt_uint(char *dst, uint64_t v, uint32_t radix, bool upper,
                           size_t *n)
{
    if (v == 0u) {
        dst[(*n)++] = '0';
        return;
    }
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    while (v != 0u) {
        dst[(*n)++] = dig[v % radix];
        v /= radix;
    }
}

static void owcrt_fmt_pad(char *out, size_t cap, size_t *wp, char c, size_t cnt)
{
    while (cnt-- > 0u) {
        owcrt_fmt_put(out, cap, wp, c);
    }
}

size_t ow_snprintf(char *out, size_t cap, const char *fmt, ...)
{
    __builtin_va_list ap;
    __builtin_va_start(ap, fmt);
    size_t w = 0u;
    if (out == NULL || cap == 0u) {
        cap = 0u;
    }

    while (*fmt != '\0') {
        char c = *fmt++;
        if (c != '%') {
            owcrt_fmt_put(out, cap, &w, c);
            continue;
        }
        /* flags */
        bool left = false, plus = false, space = false, alt = false, zero = false;
        while (*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#'
               || *fmt == '0') {
            switch (*fmt++) {
            case '-': left = true;  break;
            case '+': plus = true;  break;
            case ' ': space = true; break;
            case '#': alt  = true;  break;
            case '0': zero = true;  break;
            }
        }
        /* width */
        uint32_t width = 0u;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10u + (uint32_t)(*fmt - '0');
            fmt++;
        }
        /* precision */
        uint32_t prec = 0u;
        bool have_prec = false;
        if (*fmt == '.') {
            have_prec = true;
            fmt++;
            while (*fmt >= '0' && *fmt <= '9') {
                prec = prec * 10u + (uint32_t)(*fmt - '0');
                fmt++;
            }
        }
        /* length modifiers (2=long long family, 3=size_t/intmax family) */
        size_t llen = 0u;
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'z' || *fmt == 'j'
               || *fmt == 't' || *fmt == 'L') {
            if (*fmt == 'l') {
                llen = (llen == 1u) ? 2u : 1u;
            } else if (*fmt == 'z' || *fmt == 'j' || *fmt == 't') {
                llen = 3u;
            }
            /* 'h'/'hh' and 'L' leave llen unchanged (promoted to int) */
            fmt++;
        }
        char spec = *fmt++;
        char numbuf[24];
        bool neg = false;
        uint64_t uv = 0u;
        const char *ps = NULL;

        if (spec == 'c') {
            ps = NULL;
            /* handled below */
        } else if (spec == 's') {
            ps = __builtin_va_arg(ap, const char *);
            if (ps == NULL) {
                ps = "(null)";
            }
        } else if (spec == 'p') {
            uv = (uint64_t)(uintptr_t)__builtin_va_arg(ap, void *);
            spec = 'x';
            alt = true;
            zero = (zero && !left && !have_prec);
        } else if (spec == 'd' || spec == 'i') {
            int64_t sv;
            if (llen == 2u || llen == 3u) {
                sv = __builtin_va_arg(ap, int64_t);
            } else if (llen == 1u) {
                sv = (int64_t)__builtin_va_arg(ap, long);
            } else {
                sv = (int64_t)__builtin_va_arg(ap, int);
            }
            if (sv == INT64_MIN) {
                neg = true;
                uv = (uint64_t)(INT64_MIN);
                uv = 0u - uv;
            } else if (sv < 0) {
                neg = true;
                uv = (uint64_t)(-sv);
            } else {
                uv = (uint64_t)sv;
            }
        } else if (spec == 'u' || spec == 'o' || spec == 'x' || spec == 'X') {
            if (llen == 2u || llen == 3u) {
                uv = __builtin_va_arg(ap, uint64_t);
            } else if (llen == 1u) {
                uv = (uint64_t)__builtin_va_arg(ap, unsigned long);
            } else {
                uv = (uint64_t)__builtin_va_arg(ap, unsigned int);
            }
        } else if (spec == '%') {
            owcrt_fmt_put(out, cap, &w, '%');
            continue;
        } else {
            /* unknown specifier: emit as-is */
            owcrt_fmt_put(out, cap, &w, '%');
            owcrt_fmt_put(out, cap, &w, spec);
            continue;
        }

        size_t dg = 0u;
        uint32_t radix = 10u;
        bool upper = false;
        if (spec == 'x' || spec == 'X' || spec == 'p') {
            radix = 16u;
        } else if (spec == 'o') {
            radix = 8u;
        }
        if (spec == 'X') {
            upper = true;
        }

        size_t body = 0u;
        if (spec == 'c') {
            char cc = (char)__builtin_va_arg(ap, int);
            body = 1u;
            numbuf[0] = cc;
        } else if (spec == 's') {
            size_t l = 0u;
            while (ps[l] != '\0') {
                l++;
            }
            if (have_prec && l > prec) {
                l = prec;
            }
            body = l;
            ps = ps; /* used below */
        } else {
            owcrt_fmt_uint(numbuf, uv, radix, upper, &dg);
            owcrt_fmt_reverse(numbuf, dg);
            if (have_prec && prec > dg) {
                /* leading zeros for numeric precision */
                uint32_t extra = prec - dg;
                /* shift digits */ 
                for (size_t i = dg; i-- > 0u;) {
                    numbuf[i + extra] = numbuf[i];
                }
                for (uint32_t i = 0u; i < extra; i++) {
                    numbuf[i] = '0';
                }
                dg += extra;
            }
            body = dg;
        }

        /* sign / prefix */
        size_t sig = 0u;
        char pg[3];
        if (spec == 'd' || spec == 'i') {
            if (neg) {
                pg[sig++] = '-';
            } else if (plus) {
                pg[sig++] = '+';
            } else if (space) {
                pg[sig++] = ' ';
            }
        } else if (alt && radix == 16u && uv != 0u) {
            pg[sig++] = '0';
            pg[sig++] = upper ? 'X' : 'x';
        } else if (alt && radix == 8u && uv != 0u) {
            pg[sig++] = '0';
        }

        size_t total = sig + body;
        size_t pad   = (width > total) ? (size_t)(width - total) : 0u;

        if (!left && !zero) {
            owcrt_fmt_pad(out, cap, &w, ' ', pad);
        }
        for (size_t i = 0u; i < sig; i++) {
            owcrt_fmt_put(out, cap, &w, pg[i]);
        }
        if (left) {
            if (zero) {
                /* "0" padding with '-' flag is a spec violation; pad spaces */
            }
            if (spec != 'c' && spec != 's') {
                for (size_t i = 0u; i < body; i++) {
                    owcrt_fmt_put(out, cap, &w, numbuf[i]);
                }
            } else if (spec == 's') {
                for (size_t i = 0u; i < body; i++) {
                    owcrt_fmt_put(out, cap, &w, ps[i]);
                }
            } else {
                for (size_t i = 0u; i < body; i++) {
                    owcrt_fmt_put(out, cap, &w, numbuf[i]);
                }
            }
            owcrt_fmt_pad(out, cap, &w, ' ', pad);
        } else {
            if (zero) {
                owcrt_fmt_pad(out, cap, &w, '0', pad);
            }
            if (spec != 'c' && spec != 's') {
                for (size_t i = 0u; i < body; i++) {
                    owcrt_fmt_put(out, cap, &w, numbuf[i]);
                }
            } else if (spec == 's') {
                for (size_t i = 0u; i < body; i++) {
                    owcrt_fmt_put(out, cap, &w, ps[i]);
                }
            } else {
                for (size_t i = 0u; i < body; i++) {
                    owcrt_fmt_put(out, cap, &w, numbuf[i]);
                }
            }
        }
    }

    __builtin_va_end(ap);
    if (cap > 0u) {
        if (w < cap) {
            out[w] = '\0';
        } else {
            out[cap - 1u] = '\0';
        }
    }
    return w;
}

/* ------------------------------------------------------------------ */
/*  SUTF-8 minimals                                                    */
/* ------------------------------------------------------------------ */

size_t ow_utf8_seqlen(uint32_t cp)
{
    if (cp <= 0x7Fu) {
        return 1u;
    }
    if (cp <= 0x7FFu) {
        return 2u;
    }
    if (cp <= 0xFFFFu) {
        return 3u;
    }
    if (cp <= 0x10FFFFu) {
        return 4u;
    }
    return 0u;
}

uint32_t ow_utf8_decode(const uint8_t *s, size_t n, size_t *consumed)
{
    if (s == NULL || n == 0u) {
        if (consumed != NULL) {
            *consumed = 0u;
        }
        return 0u;
    }
    uint32_t cp = s[0];
    size_t   need = 0u;
    if (cp < 0x80u) {
        need = 1u;
    } else if ((cp & 0xE0u) == 0xC0u) {
        need = 2u; cp &= 0x1Fu;
    } else if ((cp & 0xF0u) == 0xE0u) {
        need = 3u; cp &= 0x0Fu;
    } else if ((cp & 0xF8u) == 0xF0u) {
        need = 4u; cp &= 0x07u;
    } else {
        if (consumed != NULL) {
            *consumed = 1u;
        }
        return 0xFFFFFFFFu;  /* invalid lead byte */
    }
    if (n < need) {
        if (consumed != NULL) {
            *consumed = 0u;
        }
        return 0xFFFFFFFFu;
    }
    for (size_t i = 1u; i < need; i++) {
        if ((s[i] & 0xC0u) != 0x80u) {
            if (consumed != NULL) {
                *consumed = i;
            }
            return 0xFFFFFFFFu;
        }
        cp = (cp << 6u) | (uint32_t)(s[i] & 0x3Fu);
    }
    if (consumed != NULL) {
        *consumed = need;
    }
    return cp;
}

size_t ow_utf8_encode(uint32_t cp, uint8_t *out)
{
    if (out == NULL || cp > 0x10FFFFu) {
        return 0u;
    }
    if (cp <= 0x7Fu) {
        out[0] = (uint8_t)cp;
        return 1u;
    }
    if (cp <= 0x7FFu) {
        out[0] = (uint8_t)(0xC0u | (cp >> 6));
        out[1] = (uint8_t)(0x80u | (cp & 0x3Fu));
        return 2u;
    }
    if (cp <= 0xFFFFu) {
        out[0] = (uint8_t)(0xE0u | (cp >> 12));
        out[1] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (uint8_t)(0x80u | (cp & 0x3Fu));
        return 3u;
    }
    out[0] = (uint8_t)(0xF0u | (cp >> 18));
    out[1] = (uint8_t)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (uint8_t)(0x80u | (cp & 0x3Fu));
    return 4u;
}

size_t ow_utf8_strlen(const uint8_t *s)
{
    size_t count = 0u;
    size_t i = 0u;
    while (s[i] != 0u) {
        uint32_t cp = ow_utf8_decode(s + i, 4u, NULL);
        size_t k = ow_utf8_seqlen(cp);
        if (k == 0u) {
            i++;
        } else {
            i += k;
        }
        count++;
    }
    return count;
}