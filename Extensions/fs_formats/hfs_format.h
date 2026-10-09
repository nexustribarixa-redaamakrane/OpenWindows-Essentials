/*
 * hfs_format.h - Macintosh HFS and HFS+ (Mac OS Extended) Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef HFS_FORMAT_H
#define HFS_FORMAT_H

#include <stdint.h>

#define HFS_MDB_MAGIC      0x4244u /* "BD" in big-endian */
#define HFSPLUS_VOL_MAGIC  0x482Bu /* "H+" in big-endian */
#define HFSX_VOL_MAGIC     0x4858u /* "HX" in big-endian */

#pragma pack(push, 1)
/* HFS Master Directory Block at offset 1024 */
typedef struct {
    uint16_t drSigWord;             /* 0x4244 "BD" */
    uint32_t drCrDate;              /* Creation date */
    uint32_t drLsMod;               /* Last modification */
    uint16_t drAtrb;                /* Volume attributes */
    uint16_t drNmFls;               /* Number of files */
    uint16_t drVBMSt;               /* First allocation block */
    uint16_t drAllocPtr;
    uint16_t drNmAlBlks;            /* Number of allocation blocks */
    uint32_t drAlBlkSiz;            /* Allocation block size */
    uint32_t drClpSiz;
    uint16_t drAlBlSt;
    uint32_t drNxtCNID;             /* Next catalog node ID */
    uint16_t drFreeBks;             /* Free allocation blocks */
    uint8_t  drVN[28];              /* Volume name */
} hfs_master_dir_block_t;

/* HFS+ Volume Header at offset 1024 */
typedef struct {
    uint16_t signature;             /* 0x482B 'H+' or 0x4858 'HX' */
    uint16_t version;
    uint32_t attributes;
    uint32_t lastMountedVersion;
    uint32_t journalInfoBlock;
    uint32_t createDate;
    uint32_t modifyDate;
    uint32_t backupDate;
    uint32_t checkedDate;
    uint32_t fileCount;
    uint32_t folderCount;
    uint32_t blockSize;             /* Typically 4096 */
    uint32_t totalBlocks;
    uint32_t freeBlocks;
} hfsplus_volume_header_t;
#pragma pack(pop)

#endif /* HFS_FORMAT_H */
