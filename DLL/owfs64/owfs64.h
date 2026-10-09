/*
 * owfs64.h - OpenWindows Universal Filesystem Abstraction Layer (.owd)
 *
 * Implements the master Virtual Filesystem Switch (VFS) with automatic
 * on-disk format autodetection across all supported operating system filesystems:
 * NTFS, FAT12/16/32, exFAT, ext2/3/4, Btrfs, XFS, ZFS, APFS, HFS, HFS+,
 * CDFS/ISO 9660, FFS, UFS1/2, ReFS, F2FS, UDF, SquashFS, JFS, OWFS, USFS.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWFS64_H
#define OWFS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../Extensions/vfs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Public API ───────────────────────────────────────────────── */

void         owfs64_init(void);
ow_fs_type_t owfs64_probe_media(const uint8_t *buffer, size_t size, uint64_t lba_offset);
const char  *owfs64_type_name(ow_fs_type_t type);

int32_t      owfs64_mount(const char *device_path, const char *drive_letter, ow_fs_type_t forced_type);
bool         owfs64_unmount(const char *drive_letter);
int32_t      owfs64_open(const char *path, uint32_t flags);
size_t       owfs64_read(int32_t handle, void *buffer, size_t count);
size_t       owfs64_write(int32_t handle, const void *buffer, size_t count);
bool         owfs64_close(int32_t handle);
bool         owfs64_stat(const char *path, ow_vfs_stat_t *out_stat);

uint32_t     owfs64_get_mount_count(void);
bool         owfs64_get_mount_info(uint32_t index, ow_vfs_mount_t *out_mount);

#ifdef __cplusplus
}
#endif

#endif /* OWFS64_H */
