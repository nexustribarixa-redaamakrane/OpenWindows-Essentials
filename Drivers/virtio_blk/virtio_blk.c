/*
 * virtio_blk.c - OpenWindows VirtIO Block Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation: the split virtqueue,
 * request header and status byte are file-scope static storage. The
 * transport is pluggable so the same driver drives PCI and, later, MMIO.
 */

#include "virtio_blk.h"

/* ── Tiny local helpers ───────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

/* Compiler barrier: keeps descriptor and index writes ordered. */
static void virtio_mb(void)
{
    __asm__ __volatile__("" ::: "memory");
}

/* ── Split virtqueue layout ───────────────────────────────────── */

typedef struct {
    uint64_t addr;    /* physical address of the buffer */
    uint32_t len;     /* buffer length in bytes */
    uint16_t flags;   /* VRING_DESC_F_* */
    uint16_t next;    /* next descriptor index */
} virtq_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTIO_BLK_QUEUE_SIZE];
    uint16_t used_event;
} virtq_avail_t;

typedef struct {
    uint32_t id;
    uint32_t len;
} virtq_used_elem_t;

typedef struct {
    uint16_t          flags;
    uint16_t          idx;
    virtq_used_elem_t ring[VIRTIO_BLK_QUEUE_SIZE];
    uint16_t          avail_event;
} virtq_used_t;

/* The used ring must start on a 4 KiB boundary after the avail ring. */
typedef struct {
    virtq_desc_t  desc[VIRTIO_BLK_QUEUE_SIZE];
    virtq_avail_t avail;
    uint8_t       pad[
        (4096u - (((sizeof(virtq_desc_t) * VIRTIO_BLK_QUEUE_SIZE)
                   + sizeof(virtq_avail_t)) % 4096u)) % 4096u];
    virtq_used_t  used;
} virtq_t;

#define VRING_DESC_F_NEXT   0x0001u
#define VRING_DESC_F_WRITE  0x0002u

#define VIRTIO_BLK_T_IN     0u
#define VIRTIO_BLK_T_OUT    1u
#define VIRTIO_BLK_T_FLUSH  4u

#define VIRTIO_BLK_SPIN_LIMIT 2000000u

/* Request header preceding the data payload. */
typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} virtblk_req_t;

/* ── Static DMA-backed storage ────────────────────────────────── */

static virtq_t   g_virtblk_vq __attribute__((aligned(4096)));
static virtblk_req_t g_virtblk_req;
static uint8_t   g_virtblk_status;

/* ── Config-space helpers ─────────────────────────────────────── */

static uint64_t virtblk_config64(const virtio_blk_device_t *dev, uint32_t offset)
{
    const virtio_blk_transport_t *tr = dev->transport;
    uint64_t lo = (uint64_t)tr->read32(tr->ctx, offset);
    uint64_t hi = (uint64_t)tr->read32(tr->ctx, offset + 4u);
    return lo | (hi << 32);
}

/* ── Completion polling ───────────────────────────────────────── */

static virtblk_status_t virtblk_wait(virtio_blk_device_t *dev)
{
    volatile virtq_t *vq = (volatile virtq_t *)&g_virtblk_vq;
    const virtio_blk_transport_t *tr = dev->transport;

    for (uint32_t i = 0u; i < VIRTIO_BLK_SPIN_LIMIT; ++i) {
        if ((uint16_t)vq->used.idx != dev->last_used) {
            dev->last_used = (uint16_t)vq->used.idx;
            if (tr->get_isr != NULL) {
                (void)tr->get_isr(tr->ctx);
            }
            return (g_virtblk_status == 0u) ? VIRTIO_BLK_OK
                                            : VIRTIO_BLK_ERR_IO;
        }
    }
    return VIRTIO_BLK_ERR_TIMEOUT;
}

/* ── Request submission ───────────────────────────────────────── */

static virtblk_status_t virtblk_submit(virtio_blk_device_t *dev, uint32_t type,
                                       uint64_t sector, void *buf,
                                       uint32_t bytes)
{
    volatile virtq_t *vq = (volatile virtq_t *)&g_virtblk_vq;
    const virtio_blk_transport_t *tr = dev->transport;
    uint16_t avail;
    uint16_t status_idx;

    /* Build the request header and reset the status byte. */
    g_virtblk_req.type = type;
    g_virtblk_req.reserved = 0u;
    g_virtblk_req.sector = sector;
    g_virtblk_status = 0xFFu;

    /* Descriptor 0: device-readable request header. */
    vq->desc[0].addr = (uint64_t)(uintptr_t)&g_virtblk_req;
    vq->desc[0].len = (uint32_t)sizeof(g_virtblk_req);
    vq->desc[0].flags = VRING_DESC_F_NEXT;
    vq->desc[0].next = 1u;

    if (bytes != 0u) {
        /* Descriptor 1: the data buffer (writable for reads). */
        vq->desc[1].addr = (uint64_t)(uintptr_t)buf;
        vq->desc[1].len = bytes;
        vq->desc[1].flags = (uint16_t)(VRING_DESC_F_NEXT
                            | ((type == VIRTIO_BLK_T_IN)
                               ? VRING_DESC_F_WRITE : 0u));
        vq->desc[1].next = 2u;
        status_idx = 2u;
    } else {
        status_idx = 1u;
    }

    /* Final descriptor: device-writable status byte. */
    vq->desc[status_idx].addr = (uint64_t)(uintptr_t)&g_virtblk_status;
    vq->desc[status_idx].len = 1u;
    vq->desc[status_idx].flags = VRING_DESC_F_WRITE;
    vq->desc[status_idx].next = 0u;

    virtio_mb();

    /* Publish the head descriptor (index 0) on the avail ring. */
    avail = (uint16_t)vq->avail.idx;
    vq->avail.ring[avail % dev->queue_size] = 0u;
    virtio_mb();
    vq->avail.idx = (uint16_t)(avail + 1u);
    virtio_mb();

    tr->notify(tr->ctx, 0u);
    return virtblk_wait(dev);
}

/* ── Public API ───────────────────────────────────────────────── */

virtblk_status_t virtblk_init(virtio_blk_device_t *dev,
                              const virtio_blk_transport_t *transport)
{
    const virtio_blk_transport_t *tr = transport;
    uint32_t host_features;
    uint32_t guest_features = 0u;
    uint16_t qsize;
    uint32_t pfn;

    if ((dev == NULL) || (tr == NULL)) {
        return VIRTIO_BLK_ERR_INVALID_PARAM;
    }
    if ((tr->read16 == NULL) || (tr->read32 == NULL)
        || (tr->write16 == NULL) || (tr->write32 == NULL)
        || (tr->set_status == NULL) || (tr->notify == NULL)) {
        return VIRTIO_BLK_ERR_INVALID_PARAM;
    }

    ow_memset(dev, 0, sizeof(*dev));
    dev->transport = tr;

    /* Legacy reset, then acknowledge and declare a driver. */
    tr->set_status(tr->ctx, 0u);
    dev->status = VIRTIO_STATUS_ACKNOWLEDGE;
    tr->set_status(tr->ctx, dev->status);
    dev->status |= VIRTIO_STATUS_DRIVER;
    tr->set_status(tr->ctx, dev->status);

    /* Negotiate features: only claim what the host offers. */
    host_features = tr->read32(tr->ctx, VIRTIO_PCI_HOST_FEATURES);
    if ((host_features & (1u << VIRTIO_BLK_F_FLUSH)) != 0u) {
        guest_features |= (1u << VIRTIO_BLK_F_FLUSH);
    }
    tr->write32(tr->ctx, VIRTIO_PCI_GUEST_FEATURES, guest_features);
    dev->features = guest_features;
    dev->read_only = ((host_features & (1u << VIRTIO_BLK_F_RO)) != 0u);

    /* Select queue 0 and learn its depth. */
    tr->write16(tr->ctx, VIRTIO_PCI_QUEUE_SELECT, 0u);
    qsize = tr->read16(tr->ctx, VIRTIO_PCI_QUEUE_SIZE);
    if ((qsize == 0u) || (qsize > VIRTIO_BLK_QUEUE_SIZE)) {
        return VIRTIO_BLK_ERR_UNSUPPORTED;
    }
    dev->queue_size = qsize;

    /* Publish the statically allocated split virtqueue. */
    ow_memset(&g_virtblk_vq, 0, sizeof(g_virtblk_vq));
    ow_memset(&g_virtblk_req, 0, sizeof(g_virtblk_req));
    g_virtblk_status = 0xFFu;
    virtio_mb();

    pfn = (uint32_t)(((uintptr_t)&g_virtblk_vq) >> 12);
    tr->write32(tr->ctx, VIRTIO_PCI_QUEUE_PFN, pfn);

    /* Device-specific configuration starts with the 64-bit capacity. */
    dev->capacity = virtblk_config64(dev, VIRTIO_PCI_CONFIG);
    dev->last_used = 0u;

    dev->status |= VIRTIO_STATUS_DRIVER_OK;
    tr->set_status(tr->ctx, dev->status);

    dev->initialized = true;
    return VIRTIO_BLK_OK;
}

virtblk_status_t virtblk_read(virtio_blk_device_t *dev, uint64_t sector,
                              uint32_t count, void *buf, size_t buf_size)
{
    uint64_t bytes;

    if ((dev == NULL) || !dev->initialized) {
        return VIRTIO_BLK_ERR_NOT_INITIALIZED;
    }
    if ((buf == NULL) || (count == 0u)) {
        return VIRTIO_BLK_ERR_INVALID_PARAM;
    }

    bytes = (uint64_t)count * (uint64_t)VIRTIO_BLK_SECTOR_SIZE;
    if (bytes > (uint64_t)buf_size) {
        return VIRTIO_BLK_ERR_BUFFER_TOO_SMALL;
    }
    if ((sector >= dev->capacity)
        || ((uint64_t)count > (dev->capacity - sector))) {
        return VIRTIO_BLK_ERR_OUT_OF_RANGE;
    }
    if (bytes > 0xFFFFFFFFu) {
        return VIRTIO_BLK_ERR_INVALID_PARAM;
    }
    return virtblk_submit(dev, VIRTIO_BLK_T_IN, sector, buf, (uint32_t)bytes);
}

virtblk_status_t virtblk_write(virtio_blk_device_t *dev, uint64_t sector,
                               uint32_t count, const void *buf, size_t buf_size)
{
    uint64_t bytes;

    if ((dev == NULL) || !dev->initialized) {
        return VIRTIO_BLK_ERR_NOT_INITIALIZED;
    }
    if ((buf == NULL) || (count == 0u)) {
        return VIRTIO_BLK_ERR_INVALID_PARAM;
    }
    if (dev->read_only) {
        return VIRTIO_BLK_ERR_READ_ONLY;
    }

    bytes = (uint64_t)count * (uint64_t)VIRTIO_BLK_SECTOR_SIZE;
    if (bytes > (uint64_t)buf_size) {
        return VIRTIO_BLK_ERR_BUFFER_TOO_SMALL;
    }
    if ((sector >= dev->capacity)
        || ((uint64_t)count > (dev->capacity - sector))) {
        return VIRTIO_BLK_ERR_OUT_OF_RANGE;
    }
    if (bytes > 0xFFFFFFFFu) {
        return VIRTIO_BLK_ERR_INVALID_PARAM;
    }
    return virtblk_submit(dev, VIRTIO_BLK_T_OUT, sector,
                          (void *)(uintptr_t)buf, (uint32_t)bytes);
}

virtblk_status_t virtblk_flush(virtio_blk_device_t *dev)
{
    if ((dev == NULL) || !dev->initialized) {
        return VIRTIO_BLK_ERR_NOT_INITIALIZED;
    }
    if ((dev->features & (1u << VIRTIO_BLK_F_FLUSH)) == 0u) {
        return VIRTIO_BLK_ERR_UNSUPPORTED;
    }
    return virtblk_submit(dev, VIRTIO_BLK_T_FLUSH, 0u, NULL, 0u);
}
