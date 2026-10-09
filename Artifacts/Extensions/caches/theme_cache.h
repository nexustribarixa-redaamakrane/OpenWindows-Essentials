/*
 * theme_cache.h - OpenWindows Theme Resource Cache (.kcache)
 *
 * Binary cache format with text-parseable header section.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef THEME_CACHE_H
#define THEME_CACHE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define THEME_CACHE_MAGIC    0x4B434348u  /* 'KCCH' */
#define THEME_CACHE_VERSION  1u
#define THEME_CACHE_MAX_ENTRIES 4096u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t entry_count;
    uint32_t total_size;
    uint32_t checksum;
    uint8_t  reserved[12];
} theme_cache_header_t;

typedef struct {
    uint32_t key_hash;
    uint32_t data_offset;
    uint32_t data_size;
    uint32_t ttl_seconds;
    uint64_t timestamp;
} theme_cache_entry_t;

bool theme_cache_init(void *buffer, size_t size);
bool theme_cache_lookup(const void *buffer, uint32_t key, void *out, size_t *out_size);
bool theme_cache_insert(void *buffer, uint32_t key, const void *data, size_t size, uint32_t ttl);
void theme_cache_flush(void *buffer);
uint32_t theme_cache_count(const void *buffer);

#endif /* THEME_CACHE_H */
