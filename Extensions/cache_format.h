/*
 * cache_format.h - OpenWindows Hybrid Cache (.vcache, .symcache, .fcache) Header
 *
 * Defines the hybrid cache architecture: a fixed 128-byte binary header
 * guaranteeing atomic validation and O(1) table indexing, followed by
 * dynamic cache buckets and optional plaintext metadata.
 *
 * C99 freestanding - zero dynamic heap allocation.
 */

#ifndef CACHE_FORMAT_H
#define CACHE_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define CACHE_MAGIC             0x4F574348u /* "OWCH" in little-endian */
#define CACHE_HEADER_SIZE       128u
#define CACHE_FORMAT_VERSION    0x0001u

/* Cache Sub-Types */
#define CACHE_TYPE_VOLUME_VIP   0x01u       /* .vcache: VIP volume sector trie */
#define CACHE_TYPE_SYMBOLS      0x02u       /* .symcache: Unwind symbols      */
#define CACHE_TYPE_FONT_GLYPHS  0x03u       /* .fcache: Pre-rendered glyphs   */
#define CACHE_TYPE_CONFIG_TREE  0x04u       /* .kconf: Fast compiled config   */

typedef struct {
    uint32_t key_hash;          /* CRC32c or FNV1a hash of cache key  */
    uint32_t flags;             /* Dirty, pinned, valid               */
    uint64_t data_offset;       /* File offset to cached payload      */
    uint32_t data_size;         /* Size of payload                    */
    uint32_t access_count;      /* Hot-path hit counter               */
} cache_bucket_entry_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* CACHE_MAGIC (0x4F574348)  */
    /* 0x04 */ uint16_t format_version;       /* 0x0001                    */
    /* 0x06 */ uint16_t header_size;          /* 128 bytes                 */
    /* 0x08 */ uint32_t cache_type;           /* CACHE_TYPE_* constant     */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0x7F*/
    /* 0x10 */ uint32_t bucket_count;         /* Number of cache buckets   */
    /* 0x14 */ uint32_t bucket_table_offset;  /* Offset to bucket entries  */
    /* 0x18 */ uint32_t metadata_offset;      /* Offset to plaintext / info*/
    /* 0x1C */ uint32_t metadata_size;        /* Size of metadata (bytes)  */
    /* 0x20 */ uint64_t total_cache_size;     /* Full file size            */
    /* 0x28 */ uint64_t generation_tsc;       /* Build timestamp / TSC     */
    /* 0x30 */ uint32_t cache_checksum;       /* Full CRC32c of cache file */
    /* 0x34 */ uint32_t padding[19];          /* Pad to 128 bytes          */
} cache_header_t;

static inline bool cache_header_valid(const cache_header_t *h)
{
    if (!h) return false;
    if (h->magic != CACHE_MAGIC) return false;
    if (h->header_size != CACHE_HEADER_SIZE) return false;
    if (h->cache_type < 1u || h->cache_type > 4u) return false;
    return true;
}

#endif /* CACHE_FORMAT_H */
