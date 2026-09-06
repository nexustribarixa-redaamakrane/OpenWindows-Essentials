/*
 * refsdrv.h - OpenWindows Windows Resilient File System (ReFS) Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef REFSDRV_H
#define REFSDRV_H

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
} refsdrv_state_t;

void refsdrv_init(refsdrv_state_t *state);
bool refsdrv_probe(const uint8_t *sector_buf, size_t size);
bool refsdrv_mount(refsdrv_state_t *state, const void *boot_sector);
int32_t refsdrv_open(refsdrv_state_t *state, const char *path, uint32_t flags);
size_t refsdrv_read(refsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t refsdrv_write(refsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool refsdrv_close(refsdrv_state_t *state, int32_t handle);
bool refsdrv_stat(refsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *refsdrv_get_vfs_ops(void);

#endif /* REFSDRV_H */
