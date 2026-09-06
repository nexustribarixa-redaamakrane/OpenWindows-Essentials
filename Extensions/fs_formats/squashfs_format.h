/*
 * squashfs_format.h - SquashFS Compressed Read-Only Filesystem Header
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef SQUASHFS_FORMAT_H
#define SQUASHFS_FORMAT_H

#include <stdint.h>

#define SQUASHFS_MAGIC 0x73717368u /* "hsqs" in little-endian */

#pragma pack(push, 1)
typedef struct {
    uint32_t s_magic;               /* 0x73717368 */
    uint32_t inodes;
    uint32_t mkfs_time;
    uint32_t block_size;
    uint32_t fragments;
    uint16_t compression;
    uint16_t block_log;
    uint16_t flags;
    uint16_t no_ids;
    uint16_t s_major;
    uint16_t s_minor;
    uint64_t root_inode_ref;
    uint64_t bytes_used;
    uint64_t id_table_start;
    uint64_t xattr_id_table_start;
    uint64_t inode_table_start;
    uint64_t directory_table_start;
    uint64_t fragment_table_start;
    uint64_t export_table_start;
} squashfs_super_block_t;
#pragma pack(pop)

#endif /* SQUASHFS_FORMAT_H */
