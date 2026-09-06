/*
 * vfs_types.h - OpenWindows Universal Virtual Filesystem (VFS) Switch
 *
 * Universal filesystem abstraction supporting all major operating system
 * filesystem formats via automatic superblock and signature probing.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VFS_TYPES_H
#define VFS_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Filesystem Type Identifiers ──────────────────────────────── */

typedef enum {
    OW_FS_TYPE_UNKNOWN     = 0,
    /* Native OpenWindows */
    OW_FS_TYPE_OWFS        = 1,   /* OpenWindows Native File System     */
    OW_FS_TYPE_USFS        = 2,   /* Universal Secured File System      */
    /* Microsoft Family */
    OW_FS_TYPE_FAT12       = 10,  /* MS-DOS FAT12                       */
    OW_FS_TYPE_FAT16       = 11,  /* MS-DOS / Windows FAT16             */
    OW_FS_TYPE_FAT32       = 12,  /* Windows 95/98/NT FAT32             */
    OW_FS_TYPE_EXFAT       = 13,  /* Extended FAT (exFAT / FAT64)       */
    OW_FS_TYPE_NTFS        = 14,  /* Windows NT File System             */
    OW_FS_TYPE_REFS        = 15,  /* Windows Resilient File System      */
    /* Linux Family */
    OW_FS_TYPE_EXT2        = 20,  /* Second Extended Filesystem         */
    OW_FS_TYPE_EXT3        = 21,  /* Third Extended Filesystem          */
    OW_FS_TYPE_EXT4        = 22,  /* Fourth Extended Filesystem         */
    OW_FS_TYPE_BTRFS       = 23,  /* B-tree File System                 */
    OW_FS_TYPE_XFS         = 24,  /* Silicon Graphics / Linux XFS       */
    OW_FS_TYPE_F2FS        = 25,  /* Flash-Friendly File System         */
    OW_FS_TYPE_JFS         = 26,  /* IBM Journaled File System          */
    OW_FS_TYPE_SQUASHFS    = 27,  /* Compressed Read-Only Filesystem    */
    /* Apple Family */
    OW_FS_TYPE_HFS         = 30,  /* Macintosh Hierarchical File System */
    OW_FS_TYPE_HFSPLUS     = 31,  /* Mac OS Extended (HFS+) / HFSX      */
    OW_FS_TYPE_APFS        = 32,  /* Apple File System                  */
    /* BSD & UNIX Family */
    OW_FS_TYPE_FFS         = 40,  /* Berkeley Fast File System (FFS1/2) */
    OW_FS_TYPE_UFS1        = 41,  /* Unix File System (UFS1)            */
    OW_FS_TYPE_UFS2        = 42,  /* Unix File System (UFS2)            */
    /* Solaris / OpenZFS */
    OW_FS_TYPE_ZFS         = 50,  /* Zettabyte File System (OpenZFS)    */
    /* Optical / Disc Media */
    OW_FS_TYPE_ISO9660     = 60,  /* Compact Disc File System (CDFS)    */
    OW_FS_TYPE_UDF         = 61   /* Universal Disk Format (DVD/BD)     */
} ow_fs_type_t;

/* ── Standard File Attributes & Permissions ───────────────────── */

#define OW_FS_ATTR_READONLY   0x00000001u
#define OW_FS_ATTR_HIDDEN     0x00000002u
#define OW_FS_ATTR_SYSTEM     0x00000004u
#define OW_FS_ATTR_DIRECTORY  0x00000010u
#define OW_FS_ATTR_ARCHIVE    0x00000020u
#define OW_FS_ATTR_DEVICE     0x00000040u
#define OW_FS_ATTR_SYMLINK    0x00000400u
#define OW_FS_ATTR_ENCRYPTED  0x00004000u
#define OW_FS_ATTR_COMPRESSED 0x00008000u

#define OW_VFS_PATH_MAX       260u
#define OW_VFS_NAME_MAX       128u
#define OW_VFS_MAX_MOUNTS     16u
#define OW_VFS_MAX_OPEN_FILES 64u

/* ── File Metadata (Stat) ─────────────────────────────────────── */

typedef struct {
    uint64_t     file_size;
    uint64_t     allocated_size;
    uint64_t     creation_time;
    uint64_t     last_access_time;
    uint64_t     last_write_time;
    uint32_t     attributes;
    uint32_t     inode_or_mft;
    ow_fs_type_t fs_type;
    char         name[OW_VFS_NAME_MAX];
} ow_vfs_stat_t;

/* ── VFS Operations Table ─────────────────────────────────────── */

struct ow_vfs_mount;
struct ow_vfs_file;

typedef struct {
    bool     (*mount)(struct ow_vfs_mount *vmount, const void *boot_sector);
    bool     (*unmount)(struct ow_vfs_mount *vmount);
    int32_t  (*open)(struct ow_vfs_mount *vmount, const char *path, uint32_t flags, struct ow_vfs_file *out_file);
    bool     (*close)(struct ow_vfs_file *file);
    size_t   (*read)(struct ow_vfs_file *file, uint64_t offset, void *buffer, size_t bytes_to_read);
    size_t   (*write)(struct ow_vfs_file *file, uint64_t offset, const void *buffer, size_t bytes_to_write);
    bool     (*stat)(struct ow_vfs_mount *vmount, const char *path, ow_vfs_stat_t *out_stat);
    bool     (*flush)(struct ow_vfs_mount *vmount);
} ow_vfs_ops_t;

/* ── Mount Point Structure ────────────────────────────────────── */

typedef struct ow_vfs_mount {
    uint32_t      mount_id;
    ow_fs_type_t  fs_type;
    char          mount_path[OW_VFS_PATH_MAX]; /* e.g. "C:\" or "VIP0:" */
    char          device_path[OW_VFS_PATH_MAX];/* e.g. "\Device\Harddisk0" */
    uint64_t      total_bytes;
    uint64_t      free_bytes;
    uint32_t      block_or_sector_size;
    uint32_t      cluster_size;
    bool          is_readonly;
    bool          is_active;
    const ow_vfs_ops_t *ops;
    uint8_t       fs_private[256]; /* Driver internal state */
} ow_vfs_mount_t;

/* ── Open File Descriptor ─────────────────────────────────────── */

typedef struct ow_vfs_file {
    uint32_t        handle_id;
    ow_vfs_mount_t *mount;
    uint64_t        file_pointer;
    uint64_t        file_size;
    uint64_t        extent_lba_start;
    uint32_t        flags;
    uint32_t        inode_or_record;
    bool            is_open;
    char            path[OW_VFS_PATH_MAX];
} ow_vfs_file_t;

/* ── Master VFS Manager ───────────────────────────────────────── */

typedef struct {
    ow_vfs_mount_t mounts[OW_VFS_MAX_MOUNTS];
    ow_vfs_file_t  open_files[OW_VFS_MAX_OPEN_FILES];
    uint32_t       mount_count;
    uint32_t       open_file_count;
    bool           initialized;
} ow_vfs_manager_t;

/* ── Autodetection / Probing Functions ────────────────────────── */

ow_fs_type_t ow_vfs_probe_buffer(const uint8_t *buffer, size_t size, uint64_t offset);
const char  *ow_vfs_type_to_string(ow_fs_type_t type);
const ow_vfs_ops_t *ow_vfs_get_driver_ops(ow_fs_type_t type);

void     ow_vfs_init(ow_vfs_manager_t *mgr);
int32_t  ow_vfs_mount_volume(ow_vfs_manager_t *mgr, const char *device_path, const char *mount_letter, ow_fs_type_t force_type);
bool     ow_vfs_unmount_volume(ow_vfs_manager_t *mgr, const char *mount_letter);
int32_t  ow_vfs_file_open(ow_vfs_manager_t *mgr, const char *path, uint32_t flags);
size_t   ow_vfs_file_read(ow_vfs_manager_t *mgr, int32_t handle, void *buf, size_t count);
size_t   ow_vfs_file_write(ow_vfs_manager_t *mgr, int32_t handle, const void *buf, size_t count);
bool     ow_vfs_file_close(ow_vfs_manager_t *mgr, int32_t handle);
bool     ow_vfs_get_stat(ow_vfs_manager_t *mgr, const char *path, ow_vfs_stat_t *out_stat);

#ifdef __cplusplus
}
#endif

#endif /* VFS_TYPES_H */
