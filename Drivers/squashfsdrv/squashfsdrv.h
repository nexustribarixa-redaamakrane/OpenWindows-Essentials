/*
 * squashfsdrv.h - OpenWindows SquashFS Compressed Read-Only Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef SQUASHFSDRV_H
#define SQUASHFSDRV_H

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
} squashfsdrv_state_t;

void squashfsdrv_init(squashfsdrv_state_t *state);
bool squashfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool squashfsdrv_mount(squashfsdrv_state_t *state, const void *boot_sector);
int32_t squashfsdrv_open(squashfsdrv_state_t *state, const char *path, uint32_t flags);
size_t squashfsdrv_read(squashfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t squashfsdrv_write(squashfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool squashfsdrv_close(squashfsdrv_state_t *state, int32_t handle);
bool squashfsdrv_stat(squashfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *squashfsdrv_get_vfs_ops(void);

#endif /* SQUASHFSDRV_H */
