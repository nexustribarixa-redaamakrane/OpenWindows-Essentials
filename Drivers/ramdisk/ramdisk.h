/*
 * ramdisk.h - OpenWindows Volatile RAM Disk Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef RAMDISK_H
#define RAMDISK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define RAMDISK_SECTOR_SIZE 512u
#define RAMDISK_MAX_SECTORS 65536u /* 32 MB max */

typedef struct {
    uint8_t *base;
    size_t   total_sectors;
    size_t   sector_size;
    bool     is_mounted;
} ramdisk_state_t;

void ramdisk_init(ramdisk_state_t *rd, uint8_t *buffer, size_t sector_count);
bool ramdisk_read(const ramdisk_state_t *rd, uint32_t lba, void *out, size_t count);
bool ramdisk_write(ramdisk_state_t *rd, uint32_t lba, const void *in, size_t count);

#endif /* RAMDISK_H */
