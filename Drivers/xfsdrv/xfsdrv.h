/*
 * xfsdrv.h - OpenWindows Silicon Graphics / Linux XFS Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef XFSDRV_H
#define XFSDRV_H

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
} xfsdrv_state_t;

void xfsdrv_init(xfsdrv_state_t *state);
bool xfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool xfsdrv_mount(xfsdrv_state_t *state, const void *boot_sector);
int32_t xfsdrv_open(xfsdrv_state_t *state, const char *path, uint32_t flags);
size_t xfsdrv_read(xfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t xfsdrv_write(xfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool xfsdrv_close(xfsdrv_state_t *state, int32_t handle);
bool xfsdrv_stat(xfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *xfsdrv_get_vfs_ops(void);

#endif /* XFSDRV_H */
