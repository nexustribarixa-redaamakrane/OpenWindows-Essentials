/*
 * udfdrv.h - OpenWindows Universal Disk Format (UDF) DVD/BD Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef UDFDRV_H
#define UDFDRV_H

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
} udfdrv_state_t;

void udfdrv_init(udfdrv_state_t *state);
bool udfdrv_probe(const uint8_t *sector_buf, size_t size);
bool udfdrv_mount(udfdrv_state_t *state, const void *boot_sector);
int32_t udfdrv_open(udfdrv_state_t *state, const char *path, uint32_t flags);
size_t udfdrv_read(udfdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t udfdrv_write(udfdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool udfdrv_close(udfdrv_state_t *state, int32_t handle);
bool udfdrv_stat(udfdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *udfdrv_get_vfs_ops(void);

#endif /* UDFDRV_H */
