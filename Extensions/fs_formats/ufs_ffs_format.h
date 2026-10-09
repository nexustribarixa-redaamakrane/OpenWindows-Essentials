/*
 * ufs_ffs_format.h - Berkeley Fast File System (FFS) & Unix File System (UFS1/2)
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef UFS_FFS_FORMAT_H
#define UFS_FFS_FORMAT_H

#include <stdint.h>

#define UFS1_SUPER_MAGIC 0x00011954u
#define UFS2_SUPER_MAGIC 0x19540119u
#define FFS_SUPER_MAGIC  0x00011954u

#pragma pack(push, 1)
typedef struct {
    uint32_t fs_firstfield;
    uint32_t fs_unused_1;
    uint32_t fs_sblkno;
    uint32_t fs_cblkno;
    uint32_t fs_iblkno;
    uint32_t fs_dblkno;
    uint32_t fs_cgoffset;
    uint32_t fs_cgmask;
    uint32_t fs_time;
    uint32_t fs_size;
    uint32_t fs_dsize;
    uint32_t fs_ncg;
    uint32_t fs_bsize;              /* Block size */
    uint32_t fs_fsize;              /* Fragment size */
    uint32_t fs_frag;               /* Fragments per block */
    uint32_t fs_minfree;
    uint32_t fs_rotdelay;
    uint32_t fs_rps;
    uint32_t fs_bmask;
    uint32_t fs_fmask;
    uint32_t fs_bshift;
    uint32_t fs_fshift;
    uint32_t fs_magic;              /* UFS1_SUPER_MAGIC or UFS2_SUPER_MAGIC */
} ufs_superblock_t;
#pragma pack(pop)

#endif /* UFS_FFS_FORMAT_H */
