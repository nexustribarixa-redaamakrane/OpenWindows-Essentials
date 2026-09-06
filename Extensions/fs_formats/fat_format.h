/*
 * fat_format.h - MS-DOS / Windows FAT12, FAT16, and FAT32 On-Disk Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef FAT_FORMAT_H
#define FAT_FORMAT_H

#include <stdint.h>

#define FAT_BOOT_SIGNATURE 0xAA55u

#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];
    char     oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_dir_entries;
    uint16_t total_sectors_16;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat16;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    union {
        struct {
            uint8_t  drive_number;
            uint8_t  reserved1;
            uint8_t  boot_sig;
            uint32_t volume_id;
            char     volume_label[11];
            char     fs_type[8];    /* "FAT12   " or "FAT16   " */
        } fat16;
        struct {
            uint32_t sectors_per_fat32;
            uint16_t ext_flags;
            uint16_t fs_version;
            uint32_t root_cluster;
            uint16_t fs_info_sector;
            uint16_t backup_boot_sector;
            uint8_t  reserved[12];
            uint8_t  drive_number;
            uint8_t  reserved1;
            uint8_t  boot_sig;
            uint32_t volume_id;
            char     volume_label[11];
            char     fs_type[8];    /* "FAT32   " */
        } fat32;
    } spec;
} fat_boot_sector_t;

typedef struct {
    char     filename[11];          /* 8.3 format */
    uint8_t  attributes;
    uint8_t  nt_reserved;
    uint8_t  creation_time_tenth;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat_dir_entry_t;
#pragma pack(pop)

#endif /* FAT_FORMAT_H */
