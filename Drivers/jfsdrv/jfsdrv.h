/*
 * jfsdrv.h - OpenWindows IBM Journaled File System (JFS) Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef JFSDRV_H
#define JFSDRV_H

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
} jfsdrv_state_t;

void jfsdrv_init(jfsdrv_state_t *state);
bool jfsdrv_probe(const uint8_t *sector_buf, size_t size);
bool jfsdrv_mount(jfsdrv_state_t *state, const void *boot_sector);
int32_t jfsdrv_open(jfsdrv_state_t *state, const char *path, uint32_t flags);
size_t jfsdrv_read(jfsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t jfsdrv_write(jfsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool jfsdrv_close(jfsdrv_state_t *state, int32_t handle);
bool jfsdrv_stat(jfsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *jfsdrv_get_vfs_ops(void);

#endif /* JFSDRV_H */
