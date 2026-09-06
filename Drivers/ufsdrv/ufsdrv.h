/*
 * ufsdrv.h - OpenWindows BSD Fast File System (FFS) & UFS1/2 Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef UFSDRV_H
#define UFSDRV_H

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
} ufsdrv_state_t;

void ufsdrv_init(ufsdrv_state_t *state);
bool ufsdrv_probe(const uint8_t *sector_buf, size_t size);
bool ufsdrv_mount(ufsdrv_state_t *state, const void *boot_sector);
int32_t ufsdrv_open(ufsdrv_state_t *state, const char *path, uint32_t flags);
size_t ufsdrv_read(ufsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t ufsdrv_write(ufsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool ufsdrv_close(ufsdrv_state_t *state, int32_t handle);
bool ufsdrv_stat(ufsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *ufsdrv_get_vfs_ops(void);

#endif /* UFSDRV_H */
