/*
 * btrfsdrv.h - OpenWindows B-tree File System (Btrfs) Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef BTRFSDRV_H
#define BTRFSDRV_H

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
} btrfsdrv_state_t;

void btrfsdrv_init(btrfsdrv_state_t *state);
bool btrfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool btrfsdrv_mount(btrfsdrv_state_t *state, const void *boot_sector);
int32_t btrfsdrv_open(btrfsdrv_state_t *state, const char *path, uint32_t flags);
size_t btrfsdrv_read(btrfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t btrfsdrv_write(btrfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool btrfsdrv_close(btrfsdrv_state_t *state, int32_t handle);
bool btrfsdrv_stat(btrfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *btrfsdrv_get_vfs_ops(void);

#endif /* BTRFSDRV_H */
