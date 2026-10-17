/*
 * devnull.h - OpenWindows Executive Null/Zero Device Driver (.owc)
 *
 * Implements non-POSIX NT-style Device Objects for \\Device\\Null and
 * \\Device\\Zero registered in the Object Manager directory.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef DEVNULL_H
#define DEVNULL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_INITIALIZED  0x0011AEE9U

typedef uint32_t OW_STATUS;

typedef enum _OW_NULL_DEV_MODE {
    OW_NULL_MODE_DISCARD = 0,   /* \\Device\\Null */
    OW_NULL_MODE_ZERO    = 1    /* \\Device\\Zero */
} OW_NULL_DEV_MODE;

typedef struct _OW_NULL_DEVICE {
    OW_NULL_DEV_MODE    Mode;
    uint64_t            TotalBytesRead;
    uint64_t            TotalBytesWritten;
    bool                IsInitialized;
} OW_NULL_DEVICE;

/* ── Public API ───────────────────────────────────────────────────── */

OW_STATUS OwDevNullInitialize(OW_NULL_DEVICE *Device, OW_NULL_DEV_MODE Mode);

OW_STATUS OwDevNullRead(OW_NULL_DEVICE *Device, void *Buffer, size_t Length, size_t *OutBytesRead);

OW_STATUS OwDevNullWrite(OW_NULL_DEVICE *Device, const void *Buffer, size_t Length, size_t *OutBytesWritten);

#endif /* DEVNULL_H */
