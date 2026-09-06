/*
 * ntfsdrv.h - OpenWindows Windows NT File System (NTFS) Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef NTFSDRV_H
#define NTFSDRV_H

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
} ntfsdrv_state_t;

void ntfsdrv_init(ntfsdrv_state_t *state);
bool ntfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool ntfsdrv_mount(ntfsdrv_state_t *state, const void *boot_sector);
int32_t ntfsdrv_open(ntfsdrv_state_t *state, const char *path, uint32_t flags);
size_t ntfsdrv_read(ntfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t ntfsdrv_write(ntfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool ntfsdrv_close(ntfsdrv_state_t *state, int32_t handle);
bool ntfsdrv_stat(ntfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *ntfsdrv_get_vfs_ops(void);

#endif /* NTFSDRV_H */
