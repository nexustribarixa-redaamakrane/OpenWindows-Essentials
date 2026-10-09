/*
 * zfsdrv.h - OpenWindows OpenZFS / Zettabyte File System Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ZFSDRV_H
#define ZFSDRV_H

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
} zfsdrv_state_t;

void zfsdrv_init(zfsdrv_state_t *state);
bool zfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool zfsdrv_mount(zfsdrv_state_t *state, const void *boot_sector);
int32_t zfsdrv_open(zfsdrv_state_t *state, const char *path, uint32_t flags);
size_t zfsdrv_read(zfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t zfsdrv_write(zfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool zfsdrv_close(zfsdrv_state_t *state, int32_t handle);
bool zfsdrv_stat(zfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *zfsdrv_get_vfs_ops(void);

#endif /* ZFSDRV_H */
