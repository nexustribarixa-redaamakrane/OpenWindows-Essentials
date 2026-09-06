/*
 * hfsdrv.h - OpenWindows Macintosh HFS / HFS+ Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef HFSDRV_H
#define HFSDRV_H

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
} hfsdrv_state_t;

void hfsdrv_init(hfsdrv_state_t *state);
bool hfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool hfsdrv_mount(hfsdrv_state_t *state, const void *boot_sector);
int32_t hfsdrv_open(hfsdrv_state_t *state, const char *path, uint32_t flags);
size_t hfsdrv_read(hfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t hfsdrv_write(hfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool hfsdrv_close(hfsdrv_state_t *state, int32_t handle);
bool hfsdrv_stat(hfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *hfsdrv_get_vfs_ops(void);

#endif /* HFSDRV_H */
