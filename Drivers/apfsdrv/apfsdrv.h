/*
 * apfsdrv.h - OpenWindows Apple File System (APFS) Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef APFSDRV_H
#define APFSDRV_H

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
} apfsdrv_state_t;

void apfsdrv_init(apfsdrv_state_t *state);
bool apfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool apfsdrv_mount(apfsdrv_state_t *state, const void *boot_sector);
int32_t apfsdrv_open(apfsdrv_state_t *state, const char *path, uint32_t flags);
size_t apfsdrv_read(apfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t apfsdrv_write(apfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool apfsdrv_close(apfsdrv_state_t *state, int32_t handle);
bool apfsdrv_stat(apfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *apfsdrv_get_vfs_ops(void);

#endif /* APFSDRV_H */
