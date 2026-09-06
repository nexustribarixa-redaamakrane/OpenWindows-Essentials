/*
 * fatdrv.h - OpenWindows MS-DOS / Windows FAT12/16/32 Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef FATDRV_H
#define FATDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../Extensions/vfs_types.h"

typedef struct {
    bool     is_mounted;
    uint32_t block_size;
    uint64_t total_blocks;
    uint64_t free_blocks;
    uint32_t open_count;
    char     volume_label[32];
} fatdrv_state_t;

void fatdrv_init(fatdrv_state_t *state);
bool fatdrv_probe(const uint8_t *sector_buf, size_t size);
bool fatdrv_mount(fatdrv_state_t *state, const void *boot_sector);
int32_t fatdrv_open(fatdrv_state_t *state, const char *path, uint32_t flags);
size_t fatdrv_read(fatdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t fatdrv_write(fatdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool fatdrv_close(fatdrv_state_t *state, int32_t handle);
bool fatdrv_stat(fatdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *fatdrv_get_vfs_ops(void);

#endif /* FATDRV_H */
