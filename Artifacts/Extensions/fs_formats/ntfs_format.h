/*
 * ntfs_format.h - Windows NT File System (NTFS) On-Disk Structures
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef NTFS_FORMAT_H
#define NTFS_FORMAT_H

#include <stdint.h>

#define NTFS_OEM_ID_MAGIC       0x205346544EULL /* "NTFS    " */
#define NTFS_RECORD_MAGIC_FILE  0x454C4946u     /* 'FILE' */
#define NTFS_RECORD_MAGIC_BAAD  0x44414142u     /* 'BAAD' */
#define NTFS_RECORD_MAGIC_INDX  0x58444E49u     /* 'INDX' */

#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];
    uint64_t oem_id;                /* "NTFS    " */
    uint16_t bytes_per_sector;      /* Typically 512 */
    uint8_t  sectors_per_cluster;   /* Typically 8 (4096 bytes) */
    uint16_t reserved_sectors;
    uint8_t  fats;                  /* 0 for NTFS */
    uint16_t root_entries;          /* 0 for NTFS */
    uint16_t sectors_small;         /* 0 for NTFS */
    uint8_t  media_type;
    uint16_t sectors_per_fat;       /* 0 for NTFS */
    uint16_t sectors_per_track;
    uint16_t heads;
    uint32_t hidden_sectors;
    uint32_t sectors_large;
    uint8_t  physical_drive;
    uint8_t  current_head;
    uint8_t  extended_boot_sig;
    uint8_t  reserved;
    uint64_t total_sectors;
    uint64_t mft_lcn;               /* Logical Cluster Number for $MFT */
    uint64_t mft_mirr_lcn;          /* LCN for $MFTMirr */
    int8_t   clusters_per_mft_record; /* Typically -10 -> 2^10 = 1024 bytes */
    uint8_t  reserved1[3];
    int8_t   clusters_per_index_buffer;
    uint8_t  reserved2[3];
    uint64_t volume_serial;
    uint32_t checksum;
    uint8_t  bootstrap[426];
    uint16_t signature;             /* 0xAA55 */
} ntfs_boot_sector_t;

typedef struct {
    uint32_t magic;                 /* 'FILE' */
    uint16_t usa_offset;
    uint16_t usa_count;
    uint64_t lsn;                   /* Log sequence number */
    uint16_t sequence_number;
    uint16_t link_count;
    uint16_t attrs_offset;
    uint16_t flags;                 /* 0x01 = In use, 0x02 = Directory */
    uint32_t bytes_in_use;
    uint32_t bytes_allocated;
    uint64_t base_mft_record;
    uint16_t next_attr_instance;
    uint16_t reserved;
    uint32_t mft_record_number;
} ntfs_mft_record_header_t;
#pragma pack(pop)

#endif /* NTFS_FORMAT_H */
