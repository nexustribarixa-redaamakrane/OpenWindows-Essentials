/*
 * ext4drv.h - OpenWindows Linux ext2/ext3/ext4 Filesystem Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef EXT4DRV_H
#define EXT4DRV_H

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
} ext4drv_state_t;

void ext4drv_init(ext4drv_state_t *state);
bool ext4drv_probe(const uint8_t *sector_buf, size_t size);
bool ext4drv_mount(ext4drv_state_t *state, const void *boot_sector);
int32_t ext4drv_open(ext4drv_state_t *state, const char *path, uint32_t flags);
size_t ext4drv_read(ext4drv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t ext4drv_write(ext4drv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool ext4drv_close(ext4drv_state_t *state, int32_t handle);
bool ext4drv_stat(ext4drv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *ext4drv_get_vfs_ops(void);

#endif /* EXT4DRV_H */
