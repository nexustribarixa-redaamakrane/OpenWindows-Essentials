/*
 * jfs_format.h - IBM Journaled File System (JFS) Superblock Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef JFS_FORMAT_H
#define JFS_FORMAT_H

#include <stdint.h>

#define JFS_MAGIC 0x3153464Au /* "JFS1" */

#pragma pack(push, 1)
typedef struct {
    char     s_magic[4];            /* "JFS1" */
    uint32_t s_version;
    uint64_t s_size;                /* Total size in physical blocks */
    uint32_t s_bsize;               /* Block size */
    uint16_t s_l2bsize;
    uint16_t s_l2bfactor;
    uint32_t s_pbsize;
    uint16_t s_l2pbsize;
    uint16_t s_pad;
    uint64_t s_agsize;              /* Allocation group size */
    uint32_t s_flag;
    uint32_t s_state;
} jfs_superblock_t;
#pragma pack(pop)

#endif /* JFS_FORMAT_H */
