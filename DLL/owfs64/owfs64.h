/*
 * owfs64.h - OpenWindows Universal Filesystem Abstraction Layer (.owd)
 *
 * Implements non-POSIX Virtual Filesystem Switch (VFS) with Catalogs,
 * Inodes, Superblocks, and Volume Indexing Protocol (VIP) integration.
 *
 * Conforms to OpenWindows inc/ow_vfs.h and OpenWindows-Storage specifications.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWFS64_H
#define OWFS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../Extensions/vfs_types.h"

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_FOUND        0x0011AEE2U
#define OW_ERR_ALREADY_EXISTS   0x0011AEE3U
#define OW_ERR_INSUFFICIENT     0x0011AEE4U
#define OW_ERR_ACCESS_DENIED    0x0011AEE5U
#define OW_ERR_IO               0x00000001U
#define OW_ERR_CORRUPT          0x0011A3E0U

typedef uint32_t OW_STATUS;

#define OWFS_BLOCK_SIZE         4096U
#define OWFS_NAME_MAX_BYTES     128U
#define OWFS_MAX_CATALOG_ITEMS  64U

typedef enum _OWFS_CATALOG_ITEM_TYPE {
    OWFS_TYPE_FILE    = 0x01U,
    OWFS_TYPE_CATALOG = 0x02U,
    OWFS_TYPE_DEVICE  = 0x03U
} OWFS_CATALOG_ITEM_TYPE;

typedef struct _OWFS_RECORD {
    char                    Name[OWFS_NAME_MAX_BYTES];
    uint32_t                InodeNumber;
    OWFS_CATALOG_ITEM_TYPE  ItemType;
    uint32_t                Attributes;      /* OW_FS_ATTR_* */
    uint32_t                SecurityFlags;   /* READONLY, HIDDEN */
    uint64_t                FileSize;
    uint32_t                Checksum;
} OWFS_RECORD;

typedef struct _OWFS_CATALOG_VIEW {
    char                    CatalogName[OWFS_NAME_MAX_BYTES];
    uint32_t                RecordCount;
    OWFS_RECORD             Records[OWFS_MAX_CATALOG_ITEMS];
} OWFS_CATALOG_VIEW;

/* ── Public API ───────────────────────────────────────────────────── */

OW_STATUS OwFsInitialize(void);

OW_STATUS OwFsMountVolume(const char *DevicePath, const char *DriveLetter, ow_fs_type_t ForcedType);

OW_STATUS OwFsUnmountVolume(const char *DriveLetter);

OW_STATUS OwFsEnumerateCatalog(const char *CatalogPath, OWFS_CATALOG_VIEW *OutView);

OW_STATUS OwFsCreateCatalog(const char *CatalogPath, uint32_t Attributes);

OW_STATUS OwFsCreateFileRecord(const char *FilePath, uint32_t Attributes);

OW_STATUS OwFsWriteFileData(const char *FilePath, const uint8_t *Buffer, uint32_t Size);

OW_STATUS OwFsReadFileData(const char *FilePath, uint8_t *Buffer, uint32_t MaxSize, uint32_t *BytesRead);

OW_STATUS OwFsDeleteRecord(const char *Path);

OW_STATUS OwFsQueryRecordInfo(const char *Path, OWFS_RECORD *OutRecord);

#endif /* OWFS64_H */
