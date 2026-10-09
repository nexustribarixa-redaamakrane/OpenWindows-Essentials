/*
 * f2fs_format.h - Flash-Friendly File System (F2FS) Superblock Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef F2FS_FORMAT_H
#define F2FS_FORMAT_H

#include <stdint.h>

#define F2FS_SUPER_MAGIC  0xF2F52010u
#define F2FS_BLOCK_SIZE   4096u

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;                 /* 0xF2F52010 */
    uint16_t major_ver;
    uint16_t minor_ver;
    uint32_t log_sectorsize;
    uint32_t log_sectors_per_block;
    uint32_t log_blocksize;
    uint32_t log_blocks_per_seg;
    uint32_t segs_per_sec;
    uint32_t secs_per_zone;
    uint32_t total_sections;
    uint64_t block_count;
    uint32_t section_count;
    uint32_t segment_count;
    uint32_t segment_count_ckpt;
    uint32_t segment_count_sit;
    uint32_t segment_count_nat;
    uint32_t segment_count_ssa;
    uint32_t segment_count_main;
    uint32_t checkpoint_ver;
} f2fs_superblock_t;
#pragma pack(pop)

#endif /* F2FS_FORMAT_H */
