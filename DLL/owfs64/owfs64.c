/*
 * owfs64.c - OpenWindows Universal Filesystem Abstraction Layer Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owfs64.h"

static OWFS_RECORD g_MasterCatalog[OWFS_MAX_CATALOG_ITEMS];
static uint32_t g_MasterRecordCount = 0;
static bool g_FsInitialized = false;

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
}

static bool ow_streq(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static void ow_strlcpy(char *dst, const char *src, size_t max_len)
{
    size_t i = 0;
    if (!dst || max_len == 0) return;
    if (src) {
        while (src[i] && i + 1 < max_len) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static void OwFsInitializeDefaults(void)
{
    ow_memzero(g_MasterCatalog, sizeof(g_MasterCatalog));

    /* Root Catalogs */
    ow_strlcpy(g_MasterCatalog[0].Name, "System", OWFS_NAME_MAX_BYTES);
    g_MasterCatalog[0].InodeNumber = 2;
    g_MasterCatalog[0].ItemType = OWFS_TYPE_CATALOG;
    g_MasterCatalog[0].Attributes = OW_FS_ATTR_DIRECTORY | OW_FS_ATTR_SYSTEM;
    g_MasterCatalog[0].SecurityFlags = 0x01;
    g_MasterCatalog[0].FileSize = 0;

    ow_strlcpy(g_MasterCatalog[1].Name, "Devices", OWFS_NAME_MAX_BYTES);
    g_MasterCatalog[1].InodeNumber = 3;
    g_MasterCatalog[1].ItemType = OWFS_TYPE_CATALOG;
    g_MasterCatalog[1].Attributes = OW_FS_ATTR_DIRECTORY | OW_FS_ATTR_SYSTEM;
    g_MasterCatalog[1].SecurityFlags = 0x01;
    g_MasterCatalog[1].FileSize = 0;

    ow_strlcpy(g_MasterCatalog[2].Name, "Users", OWFS_NAME_MAX_BYTES);
    g_MasterCatalog[2].InodeNumber = 4;
    g_MasterCatalog[2].ItemType = OWFS_TYPE_CATALOG;
    g_MasterCatalog[2].Attributes = OW_FS_ATTR_DIRECTORY;
    g_MasterCatalog[2].SecurityFlags = 0x00;
    g_MasterCatalog[2].FileSize = 0;

    ow_strlcpy(g_MasterCatalog[3].Name, "Temp", OWFS_NAME_MAX_BYTES);
    g_MasterCatalog[3].InodeNumber = 5;
    g_MasterCatalog[3].ItemType = OWFS_TYPE_CATALOG;
    g_MasterCatalog[3].Attributes = OW_FS_ATTR_DIRECTORY;
    g_MasterCatalog[3].SecurityFlags = 0x00;
    g_MasterCatalog[3].FileSize = 0;

    /* Root Files */
    ow_strlcpy(g_MasterCatalog[4].Name, "readme.txt", OWFS_NAME_MAX_BYTES);
    g_MasterCatalog[4].InodeNumber = 10;
    g_MasterCatalog[4].ItemType = OWFS_TYPE_FILE;
    g_MasterCatalog[4].Attributes = OW_FS_ATTR_ARCHIVE;
    g_MasterCatalog[4].SecurityFlags = 0x00;
    g_MasterCatalog[4].FileSize = 64;

    ow_strlcpy(g_MasterCatalog[5].Name, "boot.log", OWFS_NAME_MAX_BYTES);
    g_MasterCatalog[5].InodeNumber = 11;
    g_MasterCatalog[5].ItemType = OWFS_TYPE_FILE;
    g_MasterCatalog[5].Attributes = OW_FS_ATTR_SYSTEM | OW_FS_ATTR_HIDDEN;
    g_MasterCatalog[5].SecurityFlags = 0x02; /* SEC_HIDDEN */
    g_MasterCatalog[5].FileSize = 128;

    g_MasterRecordCount = 6;
    g_FsInitialized = true;
}

OW_STATUS OwFsInitialize(void)
{
    OwFsInitializeDefaults();
    return OW_SUCCESS;
}

OW_STATUS OwFsMountVolume(const char *DevicePath, const char *DriveLetter, ow_fs_type_t ForcedType)
{
    (void)DevicePath;
    (void)DriveLetter;
    (void)ForcedType;
    if (!g_FsInitialized) OwFsInitializeDefaults();
    return OW_SUCCESS;
}

OW_STATUS OwFsUnmountVolume(const char *DriveLetter)
{
    (void)DriveLetter;
    return OW_SUCCESS;
}

OW_STATUS OwFsEnumerateCatalog(const char *CatalogPath, OWFS_CATALOG_VIEW *OutView)
{
    uint32_t i;
    if (!OutView) return OW_ERR_NULL_POINTER;
    if (!g_FsInitialized) OwFsInitializeDefaults();

    ow_strlcpy(OutView->CatalogName, (CatalogPath && *CatalogPath) ? CatalogPath : "Root", OWFS_NAME_MAX_BYTES);
    OutView->RecordCount = g_MasterRecordCount;
    for (i = 0; i < g_MasterRecordCount; i++) {
        OutView->Records[i] = g_MasterCatalog[i];
    }

    return OW_SUCCESS;
}

OW_STATUS OwFsCreateCatalog(const char *CatalogPath, uint32_t Attributes)
{
    uint32_t i;
    if (!CatalogPath) return OW_ERR_NULL_POINTER;
    if (!g_FsInitialized) OwFsInitializeDefaults();

    for (i = 0; i < g_MasterRecordCount; i++) {
        if (ow_streq(g_MasterCatalog[i].Name, CatalogPath)) {
            return OW_ERR_ALREADY_EXISTS;
        }
    }

    if (g_MasterRecordCount >= OWFS_MAX_CATALOG_ITEMS) {
        return OW_ERR_INSUFFICIENT;
    }

    ow_strlcpy(g_MasterCatalog[g_MasterRecordCount].Name, CatalogPath, OWFS_NAME_MAX_BYTES);
    g_MasterCatalog[g_MasterRecordCount].InodeNumber = g_MasterRecordCount + 100;
    g_MasterCatalog[g_MasterRecordCount].ItemType = OWFS_TYPE_CATALOG;
    g_MasterCatalog[g_MasterRecordCount].Attributes = Attributes | OW_FS_ATTR_DIRECTORY;
    g_MasterCatalog[g_MasterRecordCount].SecurityFlags = 0;
    g_MasterCatalog[g_MasterRecordCount].FileSize = 0;
    g_MasterRecordCount++;

    return OW_SUCCESS;
}

OW_STATUS OwFsCreateFileRecord(const char *FilePath, uint32_t Attributes)
{
    uint32_t i;
    if (!FilePath) return OW_ERR_NULL_POINTER;
    if (!g_FsInitialized) OwFsInitializeDefaults();

    for (i = 0; i < g_MasterRecordCount; i++) {
        if (ow_streq(g_MasterCatalog[i].Name, FilePath)) {
            return OW_ERR_ALREADY_EXISTS;
        }
    }

    if (g_MasterRecordCount >= OWFS_MAX_CATALOG_ITEMS) {
        return OW_ERR_INSUFFICIENT;
    }

    ow_strlcpy(g_MasterCatalog[g_MasterRecordCount].Name, FilePath, OWFS_NAME_MAX_BYTES);
    g_MasterCatalog[g_MasterRecordCount].InodeNumber = g_MasterRecordCount + 200;
    g_MasterCatalog[g_MasterRecordCount].ItemType = OWFS_TYPE_FILE;
    g_MasterCatalog[g_MasterRecordCount].Attributes = Attributes;
    g_MasterCatalog[g_MasterRecordCount].SecurityFlags = 0;
    g_MasterCatalog[g_MasterRecordCount].FileSize = 0;
    g_MasterRecordCount++;

    return OW_SUCCESS;
}

OW_STATUS OwFsWriteFileData(const char *FilePath, const uint8_t *Buffer, uint32_t Size)
{
    uint32_t i;
    (void)Buffer;
    if (!FilePath) return OW_ERR_NULL_POINTER;
    if (!g_FsInitialized) OwFsInitializeDefaults();

    for (i = 0; i < g_MasterRecordCount; i++) {
        if (ow_streq(g_MasterCatalog[i].Name, FilePath)) {
            if (g_MasterCatalog[i].Attributes & OW_FS_ATTR_READONLY) {
                return OW_ERR_ACCESS_DENIED;
            }
            g_MasterCatalog[i].FileSize = Size;
            return OW_SUCCESS;
        }
    }

    return OwFsCreateFileRecord(FilePath, OW_FS_ATTR_ARCHIVE);
}

OW_STATUS OwFsReadFileData(const char *FilePath, uint8_t *Buffer, uint32_t MaxSize, uint32_t *BytesRead)
{
    uint32_t i;
    (void)Buffer;
    if (!FilePath || !BytesRead) return OW_ERR_NULL_POINTER;
    if (!g_FsInitialized) OwFsInitializeDefaults();

    for (i = 0; i < g_MasterRecordCount; i++) {
        if (ow_streq(g_MasterCatalog[i].Name, FilePath)) {
            uint32_t to_read = (g_MasterCatalog[i].FileSize < MaxSize) ? (uint32_t)g_MasterCatalog[i].FileSize : MaxSize;
            *BytesRead = to_read;
            return OW_SUCCESS;
        }
    }

    *BytesRead = 0;
    return OW_ERR_NOT_FOUND;
}

OW_STATUS OwFsDeleteRecord(const char *Path)
{
    uint32_t i, j;
    if (!Path) return OW_ERR_NULL_POINTER;
    if (!g_FsInitialized) OwFsInitializeDefaults();

    for (i = 0; i < g_MasterRecordCount; i++) {
        if (ow_streq(g_MasterCatalog[i].Name, Path)) {
            if (g_MasterCatalog[i].Attributes & OW_FS_ATTR_READONLY) {
                return OW_ERR_ACCESS_DENIED;
            }
            for (j = i; j + 1 < g_MasterRecordCount; j++) {
                g_MasterCatalog[j] = g_MasterCatalog[j + 1];
            }
            g_MasterRecordCount--;
            return OW_SUCCESS;
        }
    }

    return OW_ERR_NOT_FOUND;
}

OW_STATUS OwFsQueryRecordInfo(const char *Path, OWFS_RECORD *OutRecord)
{
    uint32_t i;
    if (!Path || !OutRecord) return OW_ERR_NULL_POINTER;
    if (!g_FsInitialized) OwFsInitializeDefaults();

    for (i = 0; i < g_MasterRecordCount; i++) {
        if (ow_streq(g_MasterCatalog[i].Name, Path)) {
            *OutRecord = g_MasterCatalog[i];
            return OW_SUCCESS;
        }
    }

    return OW_ERR_NOT_FOUND;
}
