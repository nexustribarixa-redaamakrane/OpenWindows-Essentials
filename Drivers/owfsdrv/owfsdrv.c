/*
 * owfsdrv.c - OpenWindows Native File System (OWFS) Storage Driver Implementation (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owfsdrv.h"

void owfs_init(owfs_driver_state_t *state)
{
    if (!state) return;
    state->open_count = 0u;
    state->is_mounted = false;
    for (size_t i = 0u; i < OWFS_MAX_OPEN_FILES; ++i) {
        state->open_files[i].is_open = false;
    }
}

bool owfs_mount(owfs_driver_state_t *state, const owfs_superblock_t *sb)
{
    if (!state || !sb) return false;
    if (sb->magic != OWFS_SUPER_MAGIC) return false;

    state->superblock = *sb;
    state->is_mounted = true;
    return true;
}

int32_t owfs_open(owfs_driver_state_t *state, uint32_t inode, uint64_t lba_start, uint64_t size)
{
    if (!state || !state->is_mounted) return -1;

    for (size_t i = 0u; i < OWFS_MAX_OPEN_FILES; ++i) {
        if (!state->open_files[i].is_open) {
            state->open_files[i].inode_id = inode;
            state->open_files[i].extent_lba_start = lba_start;
            state->open_files[i].file_size = size;
            state->open_files[i].extent_block_count = (uint32_t)((size + OWFS_BLOCK_SIZE - 1) / OWFS_BLOCK_SIZE);
            state->open_files[i].is_open = true;
            state->open_count++;
            return (int32_t)i;
        }
    }
    return -1;
}

bool owfs_close(owfs_driver_state_t *state, int32_t handle)
{
    if (!state || handle < 0 || handle >= (int32_t)OWFS_MAX_OPEN_FILES) return false;
    if (!state->open_files[handle].is_open) return false;

    state->open_files[handle].is_open = false;
    if (state->open_count > 0u) state->open_count--;
    return true;
}
