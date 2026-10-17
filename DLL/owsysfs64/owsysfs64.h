/*
 * owsysfs64.h - OpenWindows Object Namespace & Device Registry Library (.owd)
 *
 * Replaces POSIX /sys synthetic filesystem with OpenWindows Object Manager
 * namespace traversal and PnP Hardware Device Registry queries.
 *
 * Conforms to ow_object.h ("Everything is an Object").
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSYSFS64_H
#define OWSYSFS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_FOUND        0x0011AEE2U
#define OW_ERR_INSUFFICIENT     0x0011AEE4U

typedef uint32_t OW_STATUS;

#define OW_OBJ_MAX_PATH_LEN     64U
#define OW_OBJ_MAX_RECORDS      32U

typedef enum _OW_DEVICE_CLASS {
    OW_DEV_CLASS_STORAGE    = 1,
    OW_DEV_CLASS_DISPLAY    = 2,
    OW_DEV_CLASS_NETWORK    = 3,
    OW_DEV_CLASS_INPUT      = 4,
    OW_DEV_CLASS_BUS_BRIDGE = 5,
    OW_DEV_CLASS_SYSTEM     = 6
} OW_DEVICE_CLASS;

typedef struct _OW_DEVICE_NODE {
    char                DeviceName[OW_OBJ_MAX_PATH_LEN];
    char                HardwareId[OW_OBJ_MAX_PATH_LEN];
    OW_DEVICE_CLASS     DeviceClass;
    uint32_t            VendorId;
    uint32_t            DeviceId;
    uint32_t            IrqLine;
    uint64_t            BaseAddress;
    uint32_t            AddressLength;
    bool                IsOnline;
} OW_DEVICE_NODE;

typedef struct _OW_OBJECT_RECORD {
    char                Path[OW_OBJ_MAX_PATH_LEN];
    char                TypeName[24];
    uint32_t            HandleCount;
    uint32_t            SecurityFlags;
} OW_OBJECT_RECORD;

/* ── Public API ───────────────────────────────────────────────────── */

OW_STATUS OwSysQueryDeviceList(OW_DEVICE_NODE *OutBuffer, uint32_t MaxCount, uint32_t *OutCount);

OW_STATUS OwSysRegisterDeviceNode(const OW_DEVICE_NODE *Node);

OW_STATUS OwSysQueryObjectNamespace(const char *RootPath, OW_OBJECT_RECORD *OutRecords, uint32_t MaxRecords, uint32_t *OutCount);

#endif /* OWSYSFS64_H */
