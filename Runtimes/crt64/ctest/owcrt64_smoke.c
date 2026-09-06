/*
 * owcrt64_smoke.c - hosted self-test for the owcrt64.owd core runtime.
 *
 * Compiles Runtimes/crt64/owcrt64.c together with this file against the
 * host CRT (which supplies assert/malloc for the facsimile only). The k64
 * memory API is a local facsimile that hands real host blocks to the
 * allocator; the module itself never touches host malloc on its own code
 * paths. Runs: mem/str primitives, ow_snprintf, SUTF-8 helpers, the
 * segmented allocator (round-trips, realloc, large blocks, balance stats),
 * atexit/__cxa_* ordering, TLS registry, and the canary overflow guard.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "owcrt64.h"
#include "kernel64.h"

/* ------------------------------------------------------------------ */
/*  k64 facsimile (host memory)                                       */
/* ------------------------------------------------------------------ */

static bool fake_k64_ready = false;

k64_status_t k64_initialize_api(void)
{
    fake_k64_ready = true;
    return K64_OK;
}

k64_status_t k64_query_system_time(uint64_t *out_tsc)
{
    if (out_tsc) *out_tsc = (uint64_t)0x11223344;
    return K64_OK;
}

k64_status_t k64_request_memory_block(size_t size, void **out_ptr)
{
    if (!fake_k64_ready || out_ptr == NULL) return K64_ERR_NOT_INITIALIZED;
    size_t alloc = size + 64u;
    unsigned char *p = (unsigned char *)malloc(alloc);
    if (p == NULL) return K64_ERR_OUT_OF_MEMORY;
    uintptr_t a = (uintptr_t)p;
    uintptr_t aligned = (a + 63u) & ~(uintptr_t)63u;
    *out_ptr = (void *)aligned;
    return K64_OK;
}

k64_status_t k64_release_memory_block(void *ptr)
{
    (void)ptr;
    return K64_OK;
}

k64_status_t k64_raise_system_exception(uint32_t bancode, uint64_t subcode)
{
    (void)bancode; (void)subcode;
    return K64_ERR_BANCODE_RAISED;
}

bool k64_api_ready(void) { return fake_k64_ready; }
void k64_get_version(uint32_t *m, uint32_t *i, uint32_t *p)
{ if (m) *m = 1; if (i) *i = 0; if (p) *p = 0; }
k64_status_t k64_get_system_time_full(k64_system_time_t *o)
{ if (o) { o->tsc = 0; o->ticks_per_ms = 1000; o->seconds = 1; o->milliseconds = 0; } return K64_OK; }

/* ------------------------------------------------------------------ */
/*  atexit/cxa ordering probes                                        */
/* ------------------------------------------------------------------ */

static char probe_log[64];
static size_t probe_len;

static void probe_cb(void *arg)
{
    const char *s = (const char *)arg;
    if (probe_len + 2u < sizeof(probe_log)) {
        probe_log[probe_len++] = *s;
        probe_log[probe_len] = '\0';
    }
}

static void probe_plain(void)
{
    probe_log[probe_len++] = 'P';
    probe_log[probe_len] = '\0';
}

static int tls_events;

static int ev_start(void)
{
    tls_events = 0;
    return 0;
}

static void ev_thr(uintptr_t thr)
{
    static uintptr_t faux[2];
    static int busy;
    if (!busy) {
        busy = 1;
        faux[0] = (uintptr_t)&faux[0];
        faux[1] = (uintptr_t)&faux[1];
        busy = 0;
    }
    (void)thr;
    (void)ev_start;
}

int main(void)
{
    owcrt64_status_t st = owcrt64_module_init();
    assert(st == OWCRT64_OK);
    assert(owcrt64_abi_major() == 1u);

    /* ---- mem/str primitives ---- */
    {
        uint8_t buf[64], ref[64];
        for (size_t i = 0; i < 64u; i++) ref[i] = (uint8_t)(i * 3u + 1u);
        ow_memcpy(buf, ref, 64);
        assert(ow_memcmp(buf, ref, 64) == 0);
        ow_memset(buf, 0xAA, 64);
        for (size_t i = 0; i < 64u; i++) assert(buf[i] == 0xAA);
        ow_memmove(buf + 8, buf, 32);
        assert(buf[0] == 0xAA);
        assert(ow_memcpy(buf, "Hello", 6) == buf);
        assert(ow_strlen("Hello") == 5u);
        assert(ow_strcmp("abc", "abd") < 0);
        assert(ow_strncmp("abc", "abcXYZ", 3) == 0);
        char nc[8];
        ow_strncpy(nc, "ab", 8);
        assert(nc[0] == 'a' && nc[1] == 'b' && nc[2] == '\0' && nc[7] == '\0');
        /* overlapping forward + reverse memmove */
        uint8_t mv[16];
        for (size_t i = 0; i < 16u; i++) mv[i] = (uint8_t)i;
        ow_memmove(mv + 2, mv, 12u);
        assert(mv[2] == 0 && mv[13] == 11);
    }

    /* ---- ow_snprintf ---- */
    {
        char b[128];
        size_t n;
        n = ow_snprintf(b, sizeof(b), "%d|%u|%x|%X|%o", -12345, 4000000000u, 0xdead,
                        0xdead, 8u);
        assert(strcmp(b, "-12345|4000000000|dead|DEAD|10") == 0);
        assert(n == strlen(b));
        ow_snprintf(b, sizeof(b), "%08x", 0x1234);
        assert(strcmp(b, "00001234") == 0);
        ow_snprintf(b, sizeof(b), "%-6s|%6s|%c|%%", "ab", "cd", 'Q');
        assert(strcmp(b, "ab    |    cd|Q|%") == 0);
        ow_snprintf(b, sizeof(b), "%i %lld %+.4d %04d1", -7, (unsigned long long)-8, 42, 7);
        assert(strcmp(b, "-7 -8 +0042 00071") == 0);
        ow_snprintf(b, sizeof(b), "%#x %#o", 0xf, 8u);
        assert(strcmp(b, "0xf 010") == 0);
        ow_snprintf(b, sizeof(b), "%p", (void *)0x1234);
        assert(strncmp(b, "0x1234", 6) == 0);
        ow_snprintf(b, 4, "%d%d", 12345, 6789);   /* truncation */
        assert(strlen(b) <= 3u);
        ow_snprintf(b, sizeof(b), "%.2s|%.5d", "abcdef", 42);
        assert(strcmp(b, "ab|00042") == 0);
    }

    /* ---- SUTF-8 helpers ---- */
    {
        uint8_t enc[4];
        size_t k = ow_utf8_encode(0x4F60 /* 你 */, enc);
        assert(k == 3u && enc[0] == 0xE4 && enc[1] == 0xBD && enc[2] == 0xA0);
        size_t used;
        uint32_t cp = ow_utf8_decode(enc, 3u, &used);
        assert(cp == 0x4F60 && used == 3u);
        const uint8_t mixed[] = { 0x41, 0xE4, 0xBD, 0xA0, 0x00 };
        assert(ow_utf8_strlen(mixed) == 2u);
    }

    /* ---- heap round-trips ---- */
    {
        owcrt64_heap_stats_t hs;
        assert(owcrt64_heap_stats(&hs) == OWCRT64_OK);
        size_t start_allocs = hs.allocs;

        void *p1 = ow_malloc(100);
        assert(p1 != NULL && ow_malloc_usable_size(p1) >= 100u);

        ev_thr(1u);   /* exercise the never-inlined helper paths */

        uint8_t *big = (uint8_t *)ow_malloc(64u * 1024u);
        if (big) {
            for (size_t i = 0; i < 64u * 1024u; i++) big[i] = (uint8_t)i;
            assert(big[65535u] == 0xFFu);
            ow_free(big);
        }

        void *p2 = ow_calloc(16, 8);
        assert(p2 != NULL);
        uint8_t *z = (uint8_t *)p2;
        for (size_t i = 0; i < 128u; i++) assert(z[i] == 0);

        p1 = ow_realloc(p1, 1000);
        assert(p1 != NULL && ow_malloc_usable_size(p1) >= 1000u);
        ow_free(p1);
        ow_free(p2);

        void *large = ow_malloc(OWCRT64_LARGE_THRESHOLD + 1234u);
        assert(large != NULL);
        memset(large, 0x55, 1234u);
        ow_free(large);

        /* stress: many allocations / frees */
        void *ptrs[64];
        for (size_t i = 0; i < 64u; i++) ptrs[i] = ow_malloc((i % 7u) + 1u);
        for (size_t i = 0; i < 64u; i += 2u) ow_free(ptrs[i]);
        for (size_t i = 1; i < 64u; i += 2u) ow_free(ptrs[i]);

        void *zero = ow_malloc(0);
        assert(zero != NULL);
        ow_free(zero);

        assert(owcrt64_heap_stats(&hs) == OWCRT64_OK);
        assert(hs.bytes_allocated == 0u);
        assert(hs.allocs == hs.frees);
        assert(hs.guard_faults == 0u);
        assert(hs.allocs > start_allocs);
    }

    /* ---- atexit / cxa ordering ---- */
    {
        probe_log[0] = '\0';
        probe_len = 0u;
        static const char c1 = '1', c2 = '2', c3 = '3';
        assert(__cxa_atexit(probe_cb, (void *)&c1, (void *)0x1) == 0);
        assert(__cxa_atexit(probe_cb, (void *)&c2, (void *)0x1) == 0);
        assert(owcrt64_atexit(probe_plain) == OWCRT64_OK);
        assert(__cxa_atexit(probe_cb, (void *)&c3, (void *)0x2) == 0);

        /* finalizing only dso=0x1 must drop the two 1/2 entries first */
        __cxa_finalize((void *)0x1);
        assert(strcmp(probe_log, "21") == 0);

        __cxa_finalize(NULL);
        assert(strcmp(probe_log, "21P3") == 0 || strcmp(probe_log, "213P") == 0);
    }

    /* ---- TLS registry ---- */
    {
        uint32_t slot;
        assert(owcrt64_tls_slot_create(64, &slot) == OWCRT64_OK);
        unsigned char *ta = (unsigned char *)owcrt64_tls_get(slot, 1001u);
        unsigned char *tb = (unsigned char *)owcrt64_tls_get(slot, 1002u);
        assert(ta != NULL && tb != NULL && ta != tb);
        ta[0] = 0x11; tb[0] = 0x22;
        assert(((unsigned char *)owcrt64_tls_get(slot, 1001u))[0] == 0x11);

        /* same thread returns the SAME block */
        assert(owcrt64_tls_get(slot, 1001u) == (void *)ta);

        owcrt64_tls_thread_exit(1001u);
        owcrt64_tls_thread_exit(1002u);
        assert(owcrt64_tls_slot_destroy(slot) == OWCRT64_OK);

        owcrt64_heap_stats_t hs;
        assert(owcrt64_heap_stats(&hs) == OWCRT64_OK);
        assert(hs.bytes_allocated == 0u);
    }

    /* ---- canary overflow guard (must be LAST: heap enters corrupt) ---- */
    {
        uint8_t *victim = (uint8_t *)ow_malloc(100);
        assert(victim != NULL);
        for (size_t i = 0; i < 112u; i++) victim[i] = 0xFF; /* overflow 12B */
        ow_free(victim);
        owcrt64_heap_stats_t hs;
        assert(owcrt64_heap_stats(&hs) == OWCRT64_OK);
        assert(hs.guard_faults == 1u);
    }

    printf("owcrt64_smoke: PASS\n");
    return 0;
}