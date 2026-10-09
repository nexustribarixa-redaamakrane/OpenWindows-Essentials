/*
 * owfsdrv.h - OpenWindows Native File System (OWFS) Storage Driver (.owc)
 *
 * Implements superblock parsing, extent block mapping, and volume indexing
 * protocol (VIP) storage integration.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWFSDRV_H
#define OWFSDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWFS_SUPER_MAGIC    0x4F574653u /* "OWFS" */
#define OWFS_BLOCK_SIZE     4096u
#define OWFS_MAX_OPEN_FILES 32u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint64_t total_blocks;
    uint64_t free_blocks;
    uint64_t root_inode_block;
    uint32_t block_size;
    uint32_t checksum_crc32c;
} owfs_superblock_t;

typedef struct {
    uint32_t inode_id;
    uint32_t flags;
    uint64_t file_size;
    uint64_t extent_lba_start;
    uint32_t extent_block_count;
    bool     is_open;
} owfs_file_handle_t;

typedef struct {
    owfs_superblock_t  superblock;
    owfs_file_handle_t open_files[OWFS_MAX_OPEN_FILES];
    uint32_t           open_count;
    bool               is_mounted;
} owfs_driver_state_t;

void owfs_init(owfs_driver_state_t *state);
bool owfs_mount(owfs_driver_state_t *state, const owfs_superblock_t *sb);
int32_t owfs_open(owfs_driver_state_t *state, uint32_t inode, uint64_t lba_start, uint64_t size);
bool owfs_close(owfs_driver_state_t *state, int32_t handle);

#endif /* OWFSDRV_H */
