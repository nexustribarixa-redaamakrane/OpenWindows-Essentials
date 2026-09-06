/*
 * f2fsdrv.h - OpenWindows Flash-Friendly File System (F2FS) Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef F2FSDRV_H
#define F2FSDRV_H

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
} f2fsdrv_state_t;

void f2fsdrv_init(f2fsdrv_state_t *state);
bool f2fsdrv_probe(const uint8_t *sector_buf, size_t size);
bool f2fsdrv_mount(f2fsdrv_state_t *state, const void *boot_sector);
int32_t f2fsdrv_open(f2fsdrv_state_t *state, const char *path, uint32_t flags);
size_t f2fsdrv_read(f2fsdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t f2fsdrv_write(f2fsdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool f2fsdrv_close(f2fsdrv_state_t *state, int32_t handle);
bool f2fsdrv_stat(f2fsdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *f2fsdrv_get_vfs_ops(void);

#endif /* F2FSDRV_H */
