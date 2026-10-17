/*
 * owsysfs64.c - OpenWindows Object Namespace & Device Registry Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owsysfs64.h"

static OW_DEVICE_NODE g_DeviceRegistry[OW_OBJ_MAX_RECORDS];
static uint32_t g_DeviceCount = 0;
static bool g_SysInitialized = false;

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
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

static void OwSysInitializeDefaults(void)
{
    ow_memzero(g_DeviceRegistry, sizeof(g_DeviceRegistry));

    /* 1. Storage Controller */
    ow_strlcpy(g_DeviceRegistry[0].DeviceName, "\\Device\\Harddisk0", OW_OBJ_MAX_PATH_LEN);
    ow_strlcpy(g_DeviceRegistry[0].HardwareId, "PCI\\VEN_1B36&DEV_0010", OW_OBJ_MAX_PATH_LEN);
    g_DeviceRegistry[0].DeviceClass = OW_DEV_CLASS_STORAGE;
    g_DeviceRegistry[0].VendorId = 0x1B36;
    g_DeviceRegistry[0].DeviceId = 0x0010;
    g_DeviceRegistry[0].IrqLine = 14;
    g_DeviceRegistry[0].BaseAddress = 0x1F0;
    g_DeviceRegistry[0].AddressLength = 8;
    g_DeviceRegistry[0].IsOnline = true;

    /* 2. Display Adapter */
    ow_strlcpy(g_DeviceRegistry[1].DeviceName, "\\Device\\Video0", OW_OBJ_MAX_PATH_LEN);
    ow_strlcpy(g_DeviceRegistry[1].HardwareId, "PCI\\VEN_1234&DEV_1111", OW_OBJ_MAX_PATH_LEN);
    g_DeviceRegistry[1].DeviceClass = OW_DEV_CLASS_DISPLAY;
    g_DeviceRegistry[1].VendorId = 0x1234;
    g_DeviceRegistry[1].DeviceId = 0x1111;
    g_DeviceRegistry[1].IrqLine = 11;
    g_DeviceRegistry[1].BaseAddress = 0xFD000000;
    g_DeviceRegistry[1].AddressLength = 0x1000000;
    g_DeviceRegistry[1].IsOnline = true;

    /* 3. Network Interface */
    ow_strlcpy(g_DeviceRegistry[2].DeviceName, "\\Device\\Netcard0", OW_OBJ_MAX_PATH_LEN);
    ow_strlcpy(g_DeviceRegistry[2].HardwareId, "PCI\\VEN_8086&DEV_100E", OW_OBJ_MAX_PATH_LEN);
    g_DeviceRegistry[2].DeviceClass = OW_DEV_CLASS_NETWORK;
    g_DeviceRegistry[2].VendorId = 0x8086;
    g_DeviceRegistry[2].DeviceId = 0x100E;
    g_DeviceRegistry[2].IrqLine = 10;
    g_DeviceRegistry[2].BaseAddress = 0xFEB00000;
    g_DeviceRegistry[2].AddressLength = 0x20000;
    g_DeviceRegistry[2].IsOnline = true;

    g_DeviceCount = 3;
    g_SysInitialized = true;
}

OW_STATUS OwSysQueryDeviceList(OW_DEVICE_NODE *OutBuffer, uint32_t MaxCount, uint32_t *OutCount)
{
    uint32_t to_copy;
    uint32_t i;
    if (!OutBuffer || !OutCount) return OW_ERR_NULL_POINTER;
    if (!g_SysInitialized) OwSysInitializeDefaults();

    to_copy = (g_DeviceCount < MaxCount) ? g_DeviceCount : MaxCount;
    for (i = 0; i < to_copy; i++) {
        OutBuffer[i] = g_DeviceRegistry[i];
    }
    *OutCount = to_copy;
    return OW_SUCCESS;
}

OW_STATUS OwSysRegisterDeviceNode(const OW_DEVICE_NODE *Node)
{
    if (!Node) return OW_ERR_NULL_POINTER;
    if (!g_SysInitialized) OwSysInitializeDefaults();
    if (g_DeviceCount >= OW_OBJ_MAX_RECORDS) return OW_ERR_INSUFFICIENT;

    g_DeviceRegistry[g_DeviceCount++] = *Node;
    return OW_SUCCESS;
}

OW_STATUS OwSysQueryObjectNamespace(const char *RootPath, OW_OBJECT_RECORD *OutRecords, uint32_t MaxRecords, uint32_t *OutCount)
{
    (void)RootPath;
    if (!OutRecords || !OutCount) return OW_ERR_NULL_POINTER;

    /* Return standard Object Manager namespace root directories */
    if (MaxRecords < 4) return OW_ERR_INSUFFICIENT;

    ow_strlcpy(OutRecords[0].Path, "\\Device", OW_OBJ_MAX_PATH_LEN);
    ow_strlcpy(OutRecords[0].TypeName, "Directory", 24);
    OutRecords[0].HandleCount = 3;
    OutRecords[0].SecurityFlags = 0;

    ow_strlcpy(OutRecords[1].Path, "\\Driver", OW_OBJ_MAX_PATH_LEN);
    ow_strlcpy(OutRecords[1].TypeName, "Directory", 24);
    OutRecords[1].HandleCount = 12;
    OutRecords[1].SecurityFlags = 0;

    ow_strlcpy(OutRecords[2].Path, "\\KernelObjects", OW_OBJ_MAX_PATH_LEN);
    ow_strlcpy(OutRecords[2].TypeName, "Directory", 24);
    OutRecords[2].HandleCount = 20;
    OutRecords[2].SecurityFlags = 0;

    ow_strlcpy(OutRecords[3].Path, "\\FileSystem", OW_OBJ_MAX_PATH_LEN);
    ow_strlcpy(OutRecords[3].TypeName, "Directory", 24);
    OutRecords[3].HandleCount = 2;
    OutRecords[3].SecurityFlags = 0;

    *OutCount = 4;
    return OW_SUCCESS;
}
