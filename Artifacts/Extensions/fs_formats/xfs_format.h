/*
 * xfs_format.h - Silicon Graphics / Linux XFS On-Disk Superblock Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef XFS_FORMAT_H
#define XFS_FORMAT_H

#include <stdint.h>

#define XFS_SB_MAGIC 0x58465342u /* "XFSB" in big-endian */

#pragma pack(push, 1)
typedef struct {
    uint32_t sb_magicnum;           /* 0x58465342 "XFSB" */
    uint32_t sb_blocksize;          /* Logical block size */
    uint64_t sb_dblocks;            /* Total data blocks */
    uint64_t sb_rblocks;            /* Realtime blocks */
    uint64_t sb_rextents;           /* Realtime extents */
    uint8_t  sb_uuid[16];           /* Filesystem UUID */
    uint64_t sb_logstart;           /* Start block of log */
    uint64_t sb_rootino;            /* Root inode number */
    uint64_t sb_rbmino;
    uint64_t sb_rsumino;
    uint32_t sb_rextsize;
    uint32_t sb_agblocks;           /* Allocation group size in blocks */
    uint32_t sb_agcount;            /* Number of allocation groups */
    uint32_t sb_rbmblocks;
    uint32_t sb_logblocks;
    uint16_t sb_versionnum;
    uint16_t sb_sectsize;
    uint16_t sb_inodesize;          /* Size of inode in bytes */
    uint16_t sb_inopblock;          /* Inodes per logical block */
    char     sb_fname[12];          /* Filesystem name */
    uint8_t  sb_blocklog;
    uint8_t  sb_sectlog;
    uint8_t  sb_inodelog;
    uint8_t  sb_inopblog;
    uint8_t  sb_agblklog;
    uint8_t  sb_rextslog;
    uint8_t  sb_inprogress;
    uint8_t  sb_imax_pct;
    uint64_t sb_icount;             /* Inode count */
    uint64_t sb_ifree;              /* Free inodes */
    uint64_t sb_fdblocks;           /* Free data blocks */
} xfs_superblock_t;
#pragma pack(pop)

#endif /* XFS_FORMAT_H */
