/*
 * refs_format.h - Microsoft Resilient File System (ReFS) Format
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef REFS_FORMAT_H
#define REFS_FORMAT_H

#include <stdint.h>

#define REFS_OEM_ID_MAGIC 0x53466552ULL /* "ReFS" */

#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];
    char     fs_name[4];            /* "ReFS" */
    uint8_t  reserved[7];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint8_t  reserved2[2];
    uint64_t total_sectors;
    uint64_t volume_serial;
} refs_boot_sector_t;
#pragma pack(pop)

#endif /* REFS_FORMAT_H */
