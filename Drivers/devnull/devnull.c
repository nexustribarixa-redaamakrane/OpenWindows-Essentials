/*
 * devnull.c - OpenWindows Executive Null/Zero Device Driver Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "devnull.h"

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
}

OW_STATUS OwDevNullInitialize(OW_NULL_DEVICE *Device, OW_NULL_DEV_MODE Mode)
{
    if (!Device) return OW_ERR_NULL_POINTER;

    Device->Mode = Mode;
    Device->TotalBytesRead = 0;
    Device->TotalBytesWritten = 0;
    Device->IsInitialized = true;

    return OW_SUCCESS;
}

OW_STATUS OwDevNullRead(OW_NULL_DEVICE *Device, void *Buffer, size_t Length, size_t *OutBytesRead)
{
    if (!Device || !OutBytesRead) return OW_ERR_NULL_POINTER;
    if (!Device->IsInitialized) return OW_ERR_NOT_INITIALIZED;

    if (Device->Mode == OW_NULL_MODE_ZERO) {
        if (!Buffer) return OW_ERR_NULL_POINTER;
        ow_memzero(Buffer, Length);
        *OutBytesRead = Length;
        Device->TotalBytesRead += Length;
    } else {
        /* Discard mode: reading returns end-of-stream (0 bytes) */
        *OutBytesRead = 0;
    }

    return OW_SUCCESS;
}

OW_STATUS OwDevNullWrite(OW_NULL_DEVICE *Device, const void *Buffer, size_t Length, size_t *OutBytesWritten)
{
    (void)Buffer;
    if (!Device || !OutBytesWritten) return OW_ERR_NULL_POINTER;
    if (!Device->IsInitialized) return OW_ERR_NOT_INITIALIZED;

    /* Sinks all bytes unconditionally */
    *OutBytesWritten = Length;
    Device->TotalBytesWritten += Length;

    return OW_SUCCESS;
}
