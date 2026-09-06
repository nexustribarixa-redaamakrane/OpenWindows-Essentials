/*
 * apfs_format.h - Apple File System (APFS) Container Superblock Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef APFS_FORMAT_H
#define APFS_FORMAT_H

#include <stdint.h>

#define APFS_CONTAINER_MAGIC 0x4253584Eu /* "NXSB" in little-endian */

#pragma pack(push, 1)
typedef struct {
    uint64_t obj_cksum;             /* Fletcher64 checksum */
    uint64_t obj_oid;               /* Object ID */
    uint64_t obj_xid;               /* Transaction ID */
    uint32_t obj_type;              /* Object type */
    uint32_t obj_subtype;
    uint32_t nx_magic;              /* 'NXSB' */
    uint32_t nx_block_size;         /* Typically 4096 */
    uint64_t nx_block_count;        /* Total blocks in container */
    uint64_t nx_features;
    uint64_t nx_readonly_compatible_features;
    uint64_t nx_incompatible_features;
    uint8_t  nx_uuid[16];
    uint64_t nx_next_oid;
    uint64_t nx_next_xid;
    uint32_t nx_xp_desc_blocks;
    uint32_t nx_xp_data_blocks;
    uint64_t nx_xp_desc_base;
    uint64_t nx_xp_data_base;
    uint32_t nx_xp_desc_next;
    uint32_t nx_xp_data_next;
    uint32_t nx_xp_desc_index;
    uint32_t nx_xp_desc_len;
    uint32_t nx_xp_data_index;
    uint32_t nx_xp_data_len;
    uint64_t nx_spaceman_oid;
    uint64_t nx_omap_oid;           /* Object map OID */
    uint64_t nx_reaper_oid;
} apfs_container_superblock_t;
#pragma pack(pop)

#endif /* APFS_FORMAT_H */
