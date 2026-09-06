/*
 * exfatdrv.h - OpenWindows Extended FAT (exFAT) Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef EXFATDRV_H
#define EXFATDRV_H

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
} exfatdrv_state_t;

void exfatdrv_init(exfatdrv_state_t *state);
bool exfatdrv_probe(const uint8_t *sector_buf, size_t size);
bool exfatdrv_mount(exfatdrv_state_t *state, const void *boot_sector);
int32_t exfatdrv_open(exfatdrv_state_t *state, const char *path, uint32_t flags);
size_t exfatdrv_read(exfatdrv_state_t *state, int32_t handle, uint64_t offset, void *buf, size_t count);
size_t exfatdrv_write(exfatdrv_state_t *state, int32_t handle, uint64_t offset, const void *buf, size_t count);
bool exfatdrv_close(exfatdrv_state_t *state, int32_t handle);
bool exfatdrv_stat(exfatdrv_state_t *state, const char *path, ow_vfs_stat_t *out_stat);

const ow_vfs_ops_t *exfatdrv_get_vfs_ops(void);

#endif /* EXFATDRV_H */
