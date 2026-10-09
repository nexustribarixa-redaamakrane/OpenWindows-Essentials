/*
 * btrfs_format.h - B-tree File System (Btrfs) On-Disk Header Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef BTRFS_FORMAT_H
#define BTRFS_FORMAT_H

#include <stdint.h>

#define BTRFS_MAGIC_BYTES       0x4D5F53665248425FULL /* "_BHRfS_M" */
#define BTRFS_SUPER_INFO_OFFSET 0x10000ULL            /* 64 KB */

#pragma pack(push, 1)
typedef struct {
    uint8_t  csum[32];              /* Checksum */
    uint8_t  fsid[16];              /* Filesystem UUID */
    uint64_t bytenr;                /* Physical address of this superblock */
    uint64_t flags;
    uint64_t magic;                 /* "_BHRfS_M" */
    uint64_t generation;
    uint64_t root;                  /* Root tree root bytenr */
    uint64_t chunk_root;            /* Chunk tree root bytenr */
    uint64_t log_root;
    uint64_t log_root_transid;
    uint64_t total_bytes;
    uint64_t bytes_used;
    uint64_t root_dir_objectid;
    uint64_t num_devices;
    uint32_t sectorsize;
    uint32_t nodesize;
    uint32_t leafsize;
    uint32_t stripesize;
    uint32_t sys_chunk_array_size;
    uint64_t chunk_root_generation;
    uint64_t compat_flags;
    uint64_t compat_ro_flags;
    uint64_t incompat_flags;
    uint16_t csum_type;
    uint8_t  root_level;
    uint8_t  chunk_root_level;
    uint8_t  log_root_level;
    uint8_t  label[256];
} btrfs_superblock_t;
#pragma pack(pop)

#endif /* BTRFS_FORMAT_H */
