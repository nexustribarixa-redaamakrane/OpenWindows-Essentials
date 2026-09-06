/*
 * vipm.h - Volume Index Partition Manager (.owc)
 *
 * Ring-0 volume index driver. Owns the sparse 44-bit hex trie (UniVIP)
 * working set and the FVIP (File Volume Indexing Protocol) hot-path cache
 * used by the storage stack. Keys are 44-bit: 4-bit volume selector in
 * the high nibble-plane, 40-bit byte-offset selector in the low planes.
 *
 * The trie is a fixed-width radix tree over 11 nibbles with a slab-style
 * node pool and a 16-slot freelist -- zero dynamic allocation.
 *
 * SuperUnicode protocol: volume labels and FVIP paths are NUL-terminated
 * SUTF-8 streams; codepoint counts and FNV-1a 64 path hashes are kept as
 * integrity metadata alongside each entry.
 *
 * BANcode protocol: every public routine returns a 32-bit diagnostic
 * codepoint from the B+ (fatal) / W+ (warning) / S+ (soft) registry
 * blocks. Fatal corruption escalates to the damage-control sentinel.
 *
 * Conforms to OWC1 binary layout (Extensions/owc_format.h).
 * C99 freestanding - stdint/stdbool/stddef only, zero heap, static pools.
 */
#ifndef OWE_VIPM_H
#define OWE_VIPM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "owc_format.h"
#include "sucs_types.h"

/* ------------------------------------------------------------------ */
/*  OWC1 Binary Header Metadata                                        */
/* ------------------------------------------------------------------ */

#define VIPM_LIB_NAME           "vipm.owc"
#define VIPM_TARGET_ARCH        0x02u        /* x86_64                  */
#define VIPM_INIT_REQUIREMENTS  (OWC_INIT_REQUIRES_VIP  | \
                                 OWC_INIT_REQUIRES_BANC | \
                                 OWC_INIT_REQUIRES_HTL)
#define VIPM_DRIVER_FLAGS       (OWC_FLAG_PNP_AWARE | \
                                 OWC_FLAG_SENTINEL_AWARE)

/* ------------------------------------------------------------------ */
/*  Sparse 44-bit Hex Trie Geometry                                    */
/* ------------------------------------------------------------------ */

#define VIPM_KEY_BITS           44u
#define VIPM_NIBBLES            11u            /* 4 bits x 11 = 44   */
#define VIPM_VOLUME_BITS        4u             /* top nibble-plane    */
#define VIPM_OFFSET_BITS        40u            /* remaining 40 bits   */
#define VIPM_BASE_SHIFT         9u             /* byte->512-sector    */
#define VIPM_MAX_NODES          256u
#define VIPM_CHILDREN           16u
#define VIPM_NULL_INDEX         0xFFFFu        /* uint16 sentinel      */
#define VIPM_NULL_PAYLOAD       0xFFFFFFFFu

/* ------------------------------------------------------------------ */
/*  Capacity                                                           */
/* ------------------------------------------------------------------ */

#define VIPM_MAX_VOLUMES        8u
#define VIPM_FVIP_MAX_ENTRIES   64u
#define VIPM_LABEL_MAX          48u
#define VIPM_PATH_MAX           96u

/* ------------------------------------------------------------------ */
/*  Status Codes - BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A7FF): Fatal index corruption                 */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal coherency warnings           */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable faults              */
/* ------------------------------------------------------------------ */

typedef uint32_t vipm_status_t;

#define VIPM_OK                         0x00000000u  /* Success            */

/* B+ Fatal */
#define VIPM_BAN_TABLE_CORRUPT          0x0011A380u  /* Manager corrupt    */
#define VIPM_BAN_NODE_CORRUPT           0x0011A381u  /* Trie node corrupt  */
#define VIPM_BAN_TRIE_OVERRUN           0x0011A382u  /* Nibble depth blown */
#define VIPM_BAN_POOL_EXHAUSTED         0x0011A383u  /* Node freelist dead */
#define VIPM_BAN_ENTRY_CORRUPT          0x0011A384u  /* FVIP entry corrupt */
#define VIPM_BAN_VOLUME_CORRUPT         0x0011A385u  /* Registry overlap   */
#define VIPM_BAN_HASH_MISMATCH          0x0011A386u  /* Path hash broken   */

/* W+ Warning */
#define VIPM_ERR_VOLUME_NEAR_FULL       0x0011AA80u  /* Entry pressure     */
#define VIPM_ERR_INDEX_STALE            0x0011AA81u  /* Trie vs table      */
#define VIPM_ERR_DUPLICATE_MAPPING      0x0011AA82u  /* Re-insert key      */

/* S+ Soft */
#define VIPM_ERR_NOT_INIT               0x0011AEC0u  /* Manager not ready  */
#define VIPM_ERR_NULL_POINTER           0x0011AEC1u  /* Null argument      */
#define VIPM_ERR_NOT_FOUND              0x0011AEC2u  /* Key/path missing   */
#define VIPM_ERR_ALREADY_EXISTS         0x0011AEC3u  /* Entry present      */
#define VIPM_ERR_TABLE_FULL             0x0011AEC4u  /* FVIP slot full     */
#define VIPM_ERR_VOLUME_LIMIT           0x0011AEC5u  /* Registry full      */
#define VIPM_ERR_INVALID_SUTF8          0x0011AEC6u  /* Path malformed     */
#define VIPM_ERR_LABEL_TOO_LONG         0x0011AEC7u  /* Label overflow     */
#define VIPM_ERR_PATH_TOO_LONG          0x0011AEC8u  /* Path overflow      */
#define VIPM_ERR_INVALID_KEY            0x0011AEC9u  /* Key out of range   */

/* ------------------------------------------------------------------ */
/*  Storage Flag Mapping (OWFS/USFS shared on-disk layout)             */
/* ------------------------------------------------------------------ */

#define VIPM_ENTRY_FILE         0x01u
#define VIPM_ENTRY_CATALOG      0x02u
#define VIPM_ENTRY_DELETED      0x80u

#define VIPM_FLAG_FILE          0x0001u
#define VIPM_FLAG_CATALOG       0x0002u
#define VIPM_FLAG_HIDDEN        0x0004u
#define VIPM_FLAG_READONLY      0x0008u
#define VIPM_FLAG_ENCRYPTED     0x0010u
#define VIPM_FLAG_SYSTEM        0x0020u
#define VIPM_FLAG_DELETED       0x8000u

/* ------------------------------------------------------------------ */
/*  Sparse Trie Node                                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint16_t child_bitmap;                 /* bit i set -> child i live  */
    uint16_t children[VIPM_CHILDREN];      /* sparse child indices       */
    uint32_t payload;                      /* terminal: FVIP entry index */
} vipm_node_t;

/* ------------------------------------------------------------------ */
/*  FVIP Entry                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    char     path[VIPM_PATH_MAX];          /* SUTF-8 stream              */
    uint64_t byte_offset;                  /* volume-relative bytes      */
    uint32_t flags;                        /* VIPM_FLAG_*                */
    uint32_t codepoint_meta;               /* SUCS count of path         */
    uint64_t path_hash;                    /* FNV-1a 64 over path bytes  */
    uint8_t  volume_id;
    bool     occupied;
} vipm_fvip_entry_t;

/* ------------------------------------------------------------------ */
/*  Volume Registry Entry                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  volume_id;
    uint64_t base_sector;                  /* absolute 512-byte LBA      */
    char     label[VIPM_LABEL_MAX];        /* SUTF-8 stream              */
    uint32_t label_codepoints;
    bool     registered;
} vipm_volume_t;

/* ------------------------------------------------------------------ */
/*  Manager Context (static, caller-owned)                             */
/* ------------------------------------------------------------------ */

typedef struct {
    /* Sparse 44-bit hex trie */
    vipm_node_t  nodes[VIPM_MAX_NODES];
    uint16_t     free_stack[VIPM_MAX_NODES];
    uint16_t     free_top;
    uint16_t     root;
    uint16_t     node_count;

    /* Volume registry */
    vipm_volume_t volumes[VIPM_MAX_VOLUMES];
    uint8_t      volume_count;

    /* FVIP entry table */
    vipm_fvip_entry_t entries[VIPM_FVIP_MAX_ENTRIES];
    uint16_t     entry_count;

    bool         initialized;

    /* Telemetry */
    uint64_t     trie_lookups;
    uint64_t     trie_inserts;
    uint64_t     trie_removes;
    uint64_t     trie_max_depth;
    uint64_t     fvip_hits;
    uint64_t     fvip_misses;
} vipm_manager_t;

/* ================================================================== */
/*  Public API                                                        */
/* ================================================================== */

/* Initialize the manager: freelist seeded, root node allocated. */
vipm_status_t vipm_init_manager(vipm_manager_t *mgr);

/* Share a volume label as SUTF-8 (validated) with an absolute base LBA. */
vipm_status_t vipm_volume_register(vipm_manager_t *mgr, uint8_t volume_id,
                                   uint64_t base_sector, const char *label);
vipm_status_t vipm_volume_resolve(const vipm_manager_t *mgr, uint8_t volume_id,
                                  uint64_t *out_base_sector);

/* Sparse key construction: 4-bit volume selector | 40-bit offset. */
uint64_t vipm_key_make(uint8_t volume_id, uint64_t byte_offset);
bool     vipm_key_valid(uint64_t key);

/* Trie-backed mapping of a 44-bit key to an FVIP entry index. */
vipm_status_t vipm_mapping_put(vipm_manager_t *mgr, uint64_t key,
                               uint32_t entry_index);
vipm_status_t vipm_mapping_get(vipm_manager_t *mgr, uint64_t key,
                               uint32_t *out_entry_index);
vipm_status_t vipm_mapping_remove(vipm_manager_t *mgr, uint64_t key);

/* FVIP path table operations (path is a NUL-terminated SUTF-8 stream). */
vipm_status_t vipm_fvip_insert(vipm_manager_t *mgr, uint8_t volume_id,
                               const char *path, uint64_t byte_offset,
                               uint32_t flags);
vipm_status_t vipm_fvip_lookup(vipm_manager_t *mgr, uint8_t volume_id,
                               const char *path,
                               vipm_fvip_entry_t *out_entry);
vipm_status_t vipm_fvip_remove(vipm_manager_t *mgr, uint8_t volume_id,
                               const char *path);

/* Storage flag conversions (OWFS/USFS shared on-disk layout). */
uint32_t vipm_flags_from_storage(uint8_t entry_type, uint32_t sec_flags);
void     vipm_flags_to_storage(uint32_t fvip_flags, uint8_t *out_entry_type,
                               uint32_t *out_sec_flags);

/* Full structural integrity walk (trie + table + hashes). */
vipm_status_t vipm_integrity_verify(const vipm_manager_t *mgr);

/* Telemetry snapshot. */
void vipm_get_telemetry(const vipm_manager_t *mgr,
                        uint64_t *out_trie_lookups,
                        uint64_t *out_trie_inserts,
                        uint64_t *out_trie_depth,
                        uint64_t *out_fvip_hits,
                        uint64_t *out_fvip_misses);

#endif /* OWE_VIPM_H */