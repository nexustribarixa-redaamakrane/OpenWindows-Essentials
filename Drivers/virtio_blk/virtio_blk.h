/*
 * virtio_blk.h - OpenWindows VirtIO Block Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VIRTIO_BLK_H
#define VIRTIO_BLK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Native virtio-blk logical sector size. */
#define VIRTIO_BLK_SECTOR_SIZE  512u
/* Capacity of the static split virtqueue (descriptors, avail and used). */
#define VIRTIO_BLK_QUEUE_SIZE   256u
/* Legacy PCI configuration-space offsets. */
#define VIRTIO_PCI_HOST_FEATURES  0x00u
#define VIRTIO_PCI_GUEST_FEATURES 0x04u
#define VIRTIO_PCI_QUEUE_PFN      0x08u
#define VIRTIO_PCI_QUEUE_SIZE     0x0Cu
#define VIRTIO_PCI_QUEUE_SELECT   0x0Eu
#define VIRTIO_PCI_QUEUE_NOTIFY   0x10u
#define VIRTIO_PCI_STATUS         0x12u
#define VIRTIO_PCI_ISR            0x13u
#define VIRTIO_PCI_CONFIG         0x14u

/* Device status bits. */
#define VIRTIO_STATUS_ACKNOWLEDGE  0x01u
#define VIRTIO_STATUS_DRIVER       0x02u
#define VIRTIO_STATUS_DRIVER_OK    0x04u
#define VIRTIO_STATUS_FAILED       0x80u

/* virtio-blk feature bits. */
#define VIRTIO_BLK_F_RO            5u
#define VIRTIO_BLK_F_FLUSH         9u

/* Status codes returned by every VirtIO block entry point. */
typedef enum {
    VIRTIO_BLK_OK = 0,
    VIRTIO_BLK_ERR_INVALID_PARAM,
    VIRTIO_BLK_ERR_NOT_INITIALIZED,
    VIRTIO_BLK_ERR_TIMEOUT,
    VIRTIO_BLK_ERR_IO,
    VIRTIO_BLK_ERR_UNSUPPORTED,
    VIRTIO_BLK_ERR_OUT_OF_RANGE,
    VIRTIO_BLK_ERR_READ_ONLY,
    VIRTIO_BLK_ERR_BUFFER_TOO_SMALL
} virtblk_status_t;

/* Pluggable VirtIO transport: PCI config-space access, status register,
 * interrupt status and queue notification. The function pointers that are
 * not strictly required by init may be NULL. */
typedef struct {
    uint8_t  (*read8)(void *ctx, uint32_t offset);
    uint16_t (*read16)(void *ctx, uint32_t offset);
    uint32_t (*read32)(void *ctx, uint32_t offset);
    void     (*write8)(void *ctx, uint32_t offset, uint8_t value);
    void     (*write16)(void *ctx, uint32_t offset, uint16_t value);
    void     (*write32)(void *ctx, uint32_t offset, uint32_t value);
    uint8_t  (*get_status)(void *ctx);
    void     (*set_status)(void *ctx, uint8_t status);
    uint32_t (*get_isr)(void *ctx);
    void     (*notify)(void *ctx, uint16_t queue);
    void      *ctx;
} virtio_blk_transport_t;

/* VirtIO block device instance. Caller owns the storage; no heap is used. */
typedef struct {
    const virtio_blk_transport_t *transport;   /* bound transport vtable   */
    uint64_t capacity;                         /* device size in sectors   */
    uint32_t features;                         /* negotiated feature bits  */
    uint16_t queue_size;                       /* device queue 0 depth     */
    uint16_t last_used;                        /* last observed used index */
    uint8_t  status;                           /* device status shadow     */
    bool     read_only;                        /* VIRTIO_BLK_F_RO offered  */
    bool     initialized;
} virtio_blk_device_t;

/* Negotiate features, publish the static virtqueue and enable the device. */
virtblk_status_t virtblk_init(virtio_blk_device_t *dev,
                              const virtio_blk_transport_t *transport);

/* Submit an NVM Read request (type 0) for `count` sectors at `sector`. */
virtblk_status_t virtblk_read(virtio_blk_device_t *dev, uint64_t sector,
                              uint32_t count, void *buf, size_t buf_size);

/* Submit an NVM Write request (type 1) for `count` sectors at `sector`. */
virtblk_status_t virtblk_write(virtio_blk_device_t *dev, uint64_t sector,
                               uint32_t count, const void *buf, size_t buf_size);

/* Submit a Flush request (type 4) when VIRTIO_BLK_F_FLUSH was negotiated. */
virtblk_status_t virtblk_flush(virtio_blk_device_t *dev);

#endif /* VIRTIO_BLK_H */
