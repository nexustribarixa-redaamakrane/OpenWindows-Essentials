/*
 * virtio_net.c - OpenWindows VirtIO Network Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "virtio_net.h"

/* ── Local helpers ────────────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

static void ow_memcpy(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

/* Compiler ordering barrier; the transport's register writes fence MMIO. */
static void virtnet_mb(void)
{
    __asm__ __volatile__("" ::: "memory");
}

/* Program descriptor/avail/used addresses for one queue. */
static void virtnet_program_queue(virtnet_dev_t *dev, uint16_t queue,
                                  virtnet_queue_t *q, uint16_t size)
{
    dev->transport->set_queue_size(dev->transport->ctx, queue, size);
    dev->transport->set_queue_addr(dev->transport->ctx, queue,
                                   (uint64_t)(uintptr_t)&q->desc[0],
                                   (uint64_t)(uintptr_t)&q->avail,
                                   (uint64_t)(uintptr_t)&q->used);
    dev->transport->set_queue_enable(dev->transport->ctx, queue, true);
}

/* Validate the transport exposes everything this driver depends on. */
static bool virtnet_transport_valid(const virtnet_transport_t *t)
{
    if (t == NULL) {
        return false;
    }
    if (t->get_features == NULL || t->set_features == NULL ||
        t->get_status == NULL || t->set_status == NULL || t->reset == NULL) {
        return false;
    }
    if (t->get_queue_size == NULL || t->set_queue_size == NULL ||
        t->set_queue_addr == NULL || t->set_queue_enable == NULL) {
        return false;
    }
    if (t->notify_queue == NULL || t->read_config == NULL) {
        return false;
    }
    return true;
}

/* ── Implementation ───────────────────────────────────────────── */

virtnet_status_t virtnet_init(virtnet_dev_t *dev,
                              const virtnet_transport_t *transport)
{
    static const uint8_t fallback_mac[6] = { 0x52u, 0x54u, 0x00u,
                                             0x12u, 0x34u, 0x56u };
    void *ctx;
    uint16_t i;
    uint16_t rx_size;
    uint16_t tx_size;
    uint64_t features;
    uint64_t negotiated;
    uint8_t  status;

    if (dev == NULL || !virtnet_transport_valid(transport)) {
        return VIRTNET_ERR_PARAM;
    }

    ow_memset(dev, 0, sizeof(*dev));
    dev->transport = transport;
    ctx = transport->ctx;

    /* Reset then walk the standard virtio status handshake. */
    transport->reset(ctx);
    status = VIRTIO_STATUS_ACKNOWLEDGE;
    transport->set_status(ctx, status);
    status |= VIRTIO_STATUS_DRIVER;
    transport->set_status(ctx, status);

    features = transport->get_features(ctx);
    negotiated = features & (VIRTIO_F_VERSION_1 | VIRTIO_NET_F_MAC |
                             VIRTIO_NET_F_STATUS);
    transport->set_features(ctx, negotiated);

    status |= VIRTIO_STATUS_FEATURES_OK;
    transport->set_status(ctx, status);
    if ((transport->get_status(ctx) & VIRTIO_STATUS_FEATURES_OK) == 0u) {
        transport->set_status(ctx, status | VIRTIO_STATUS_FAILED);
        return VIRTNET_ERR_UNSUPPORTED;
    }
    dev->features = negotiated;

    /* Station address: from device config, else the QEMU default. */
    if ((negotiated & VIRTIO_NET_F_MAC) != 0u) {
        if (!transport->read_config(ctx, 0u, dev->mac, 6u)) {
            for (i = 0; i < 6u; ++i) {
                dev->mac[i] = fallback_mac[i];
            }
        }
    } else {
        for (i = 0; i < 6u; ++i) {
            dev->mac[i] = fallback_mac[i];
        }
    }

    /* Receive queue: post every descriptor as a writable buffer. */
    rx_size = transport->get_queue_size(ctx, VIRTNET_RX_QUEUE);
    if (rx_size == 0u) {
        return VIRTNET_ERR_UNSUPPORTED;
    }
    if (rx_size > VIRTNET_QUEUE_SIZE) {
        rx_size = VIRTNET_QUEUE_SIZE;
    }
    dev->rxq.size = rx_size;
    ow_memset(&dev->rxq.desc[0], 0, sizeof(dev->rxq.desc));
    ow_memset(&dev->rxq.avail, 0, sizeof(dev->rxq.avail));
    ow_memset(&dev->rxq.used, 0, sizeof(dev->rxq.used));
    for (i = 0; i < rx_size; ++i) {
        dev->rxq.desc[i].addr  = (uint64_t)(uintptr_t)dev->rx_buf[i];
        dev->rxq.desc[i].len   = VIRTNET_FRAME_MAX;
        dev->rxq.desc[i].flags = VIRTQ_DESC_F_WRITE;
        dev->rxq.desc[i].next  = 0u;
        dev->rxq.avail.ring[i] = i;
    }
    dev->rxq.avail.flags = 0u;
    dev->rxq.avail.idx   = rx_size;
    virtnet_program_queue(dev, VIRTNET_RX_QUEUE, &dev->rxq, rx_size);

    /* Transmit queue: descriptors are published on demand by send. */
    tx_size = transport->get_queue_size(ctx, VIRTNET_TX_QUEUE);
    if (tx_size == 0u) {
        return VIRTNET_ERR_UNSUPPORTED;
    }
    if (tx_size > VIRTNET_QUEUE_SIZE) {
        tx_size = VIRTNET_QUEUE_SIZE;
    }
    dev->txq.size = tx_size;
    ow_memset(&dev->txq.desc[0], 0, sizeof(dev->txq.desc));
    ow_memset(&dev->txq.avail, 0, sizeof(dev->txq.avail));
    ow_memset(&dev->txq.used, 0, sizeof(dev->txq.used));
    dev->txq.avail.flags = 0u;
    dev->txq.avail.idx   = 0u;
    virtnet_program_queue(dev, VIRTNET_TX_QUEUE, &dev->txq, tx_size);

    if ((negotiated & VIRTIO_NET_F_STATUS) != 0u) {
        uint16_t ls = 0u;
        if (transport->read_config(ctx, 6u, &ls, 2u)) {
            dev->link_status = ls;
        }
    }

    dev->rx_last_used = 0u;
    status |= VIRTIO_STATUS_DRIVER_OK;
    transport->set_status(ctx, status);
    dev->initialized = true;
    return VIRTNET_OK;
}

virtnet_status_t virtnet_send(virtnet_dev_t *dev, const void *frame,
                              uint32_t len)
{
    virtnet_queue_t *q;
    uint16_t avail_idx;
    uint16_t used_idx;
    uint16_t desc_idx;

    if (dev == NULL || frame == NULL) {
        return VIRTNET_ERR_PARAM;
    }
    if (!dev->initialized) {
        return VIRTNET_ERR_NOT_READY;
    }
    if (len == 0u || len > VIRTNET_FRAME_MAX) {
        return VIRTNET_ERR_PARAM;
    }

    q = &dev->txq;
    avail_idx = q->avail.idx;
    used_idx  = q->used.idx;
    if ((uint16_t)(avail_idx - used_idx) >= q->size) {
        return VIRTNET_ERR_NO_SPACE;
    }

    desc_idx = (uint16_t)(avail_idx % q->size);
    ow_memcpy(dev->tx_buf[desc_idx], frame, len);
    q->desc[desc_idx].addr  = (uint64_t)(uintptr_t)dev->tx_buf[desc_idx];
    q->desc[desc_idx].len   = len;
    q->desc[desc_idx].flags = 0u;
    q->desc[desc_idx].next  = 0u;

    virtnet_mb();
    q->avail.ring[avail_idx % q->size] = desc_idx;
    virtnet_mb();
    q->avail.idx = (uint16_t)(avail_idx + 1u);
    virtnet_mb();

    dev->transport->notify_queue(dev->transport->ctx, VIRTNET_TX_QUEUE);
    return VIRTNET_OK;
}

virtnet_status_t virtnet_recv(virtnet_dev_t *dev, void *buf, uint32_t cap,
                              uint32_t *out_len)
{
    virtnet_queue_t *q;
    volatile virtq_used_t *used;
    uint16_t used_idx;
    uint16_t slot;
    uint16_t desc_idx;
    uint32_t len;
    uint16_t avail_idx;

    if (dev == NULL || buf == NULL || out_len == NULL) {
        return VIRTNET_ERR_PARAM;
    }
    if (!dev->initialized) {
        return VIRTNET_ERR_NOT_READY;
    }

    q = &dev->rxq;
    used = &q->used;

    virtnet_mb();
    used_idx = used->idx;
    if (used_idx == dev->rx_last_used) {
        return VIRTNET_ERR_NOT_READY;   /* nothing received */
    }

    slot = (uint16_t)(dev->rx_last_used % q->size);
    desc_idx = (uint16_t)(used->ring[slot].id);
    if (desc_idx >= q->size) {
        return VIRTNET_ERR_IO;
    }

    len = used->ring[slot].len;
    if (len > VIRTNET_FRAME_MAX) {
        len = VIRTNET_FRAME_MAX;
    }
    if (len > cap) {
        len = cap;   /* truncate rather than stall the ring */
    }

    ow_memcpy(buf, dev->rx_buf[desc_idx], len);
    *out_len = len;
    dev->rx_last_used = (uint16_t)(dev->rx_last_used + 1u);

    /* Return the descriptor to the device's available ring. */
    q->desc[desc_idx].len   = VIRTNET_FRAME_MAX;
    q->desc[desc_idx].flags = VIRTQ_DESC_F_WRITE;
    q->desc[desc_idx].next  = 0u;
    avail_idx = q->avail.idx;
    q->avail.ring[avail_idx % q->size] = desc_idx;
    virtnet_mb();
    q->avail.idx = (uint16_t)(avail_idx + 1u);
    virtnet_mb();

    dev->transport->notify_queue(dev->transport->ctx, VIRTNET_RX_QUEUE);
    return VIRTNET_OK;
}

virtnet_status_t virtnet_get_mac(const virtnet_dev_t *dev, uint8_t out_mac[6])
{
    uint32_t i;

    if (dev == NULL || out_mac == NULL) {
        return VIRTNET_ERR_PARAM;
    }
    if (!dev->initialized) {
        return VIRTNET_ERR_NOT_READY;
    }
    for (i = 0; i < 6u; ++i) {
        out_mac[i] = dev->mac[i];
    }
    return VIRTNET_OK;
}
