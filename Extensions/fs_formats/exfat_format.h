/*
 * exfat_format.h - Extended FAT (exFAT / FAT64) Format Specification
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef EXFAT_FORMAT_H
#define EXFAT_FORMAT_H

#include <stdint.h>

#define EXFAT_OEM_NAME_MAGIC 0x205441465845ULL /* "EXFAT   " */
#define EXFAT_BOOT_SIGNATURE 0xAA55u

#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];
    uint64_t oem_name;              /* "EXFAT   " */
    uint8_t  reserved_zero[53];
    uint64_t partition_offset;
    uint64_t volume_length_sectors;
    uint32_t fat_offset_sectors;
    uint32_t fat_length_sectors;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t root_dir_cluster;
    uint32_t volume_serial_number;
    uint16_t fs_revision;
    uint16_t volume_flags;
    uint8_t  bytes_per_sector_shift;   /* typically 9 (512) or 12 (4096) */
    uint8_t  sectors_per_cluster_shift;/* typically 3 (8) -> 32KB cluster */
    uint8_t  fat_count;
    uint8_t  drive_select;
    uint8_t  percent_in_use;
    uint8_t  reserved[7];
    uint8_t  boot_code[390];
    uint16_t boot_signature;        /* 0xAA55 */
} exfat_boot_sector_t;
#pragma pack(pop)

#endif /* EXFAT_FORMAT_H */
