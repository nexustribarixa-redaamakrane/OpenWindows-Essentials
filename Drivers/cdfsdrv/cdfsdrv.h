/*
 * cdfsdrv.h - OpenWindows Compact Disc File System (CDFS / ISO 9660) Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef CDFSDRV_H
#define CDFSDRV_H

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
} cdfsdrv_state_t;

void cdfsdrv_init(cdfsdrv_state_t *state);
bool cdfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool cdfsdrv_mount(cdfsdrv_state_t *state, const void *boot_sector);
int32_t cdfsdrv_open(cdfsdrv_state_t *state, const char *path, uint32_t flags);
size_t cdfsdrv_read(cdfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t cdfsdrv_write(cdfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool cdfsdrv_close(cdfsdrv_state_t *state, int32_t handle);
bool cdfsdrv_stat(cdfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *cdfsdrv_get_vfs_ops(void);

#endif /* CDFSDRV_H */
