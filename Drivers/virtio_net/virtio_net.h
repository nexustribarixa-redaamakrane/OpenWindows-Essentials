/*
 * virtio_net.h - OpenWindows VirtIO Network Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t virtnet_status_t;

#define VIRTNET_OK            0x00000000u  /* Operation completed     */
#define VIRTNET_ERR_PARAM     0x00000001u  /* Invalid argument        */
#define VIRTNET_ERR_NOT_READY 0x00000002u  /* Device not initialised  */
#define VIRTNET_ERR_NO_SPACE  0x00000003u  /* TX queue full           */
#define VIRTNET_ERR_IO        0x00000004u  /* Malformed used ring     */
#define VIRTNET_ERR_UNSUPPORTED 0x00000005u /* Feature negotiation    */

/* ── VirtIO feature bits ──────────────────────────────────────── */

#define VIRTIO_F_VERSION_1      (1ULL << 32)  /* Modern (1.0) device   */
#define VIRTIO_NET_F_MAC        (1ULL << 5)   /* Config has MAC        */
#define VIRTIO_NET_F_STATUS     (1ULL << 16)  /* Config has link state */

/* ── VirtIO device status bits ────────────────────────────────── */

#define VIRTIO_STATUS_ACKNOWLEDGE 0x01u
#define VIRTIO_STATUS_DRIVER      0x02u
#define VIRTIO_STATUS_DRIVER_OK   0x04u
#define VIRTIO_STATUS_FEATURES_OK 0x08u
#define VIRTIO_STATUS_FAILED      0x80u

/* ── Queue indices ────────────────────────────────────────────── */

#define VIRTNET_RX_QUEUE 0u
#define VIRTNET_TX_QUEUE 1u

/* ── Descriptor flags ─────────────────────────────────────────── */

#define VIRTQ_DESC_F_NEXT  0x1u
#define VIRTQ_DESC_F_WRITE 0x2u

/* ── Capacity limits (static storage) ─────────────────────────── */

#define VIRTNET_QUEUE_SIZE 16u      /* Descriptors per virtqueue     */
#define VIRTNET_FRAME_MAX  2048u    /* Per-descriptor data buffer    */

/* ── Pluggable transport (PCI / MMIO, legacy or modern) ───────── */

typedef struct virtnet_transport {
    void *ctx;                                  /* Transport private   */
    uint64_t (*get_features)(void *ctx);
    void     (*set_features)(void *ctx, uint64_t features);
    uint8_t  (*get_status)(void *ctx);
    void     (*set_status)(void *ctx, uint8_t status);
    void     (*reset)(void *ctx);
    uint16_t (*get_queue_size)(void *ctx, uint16_t queue);
    void     (*set_queue_size)(void *ctx, uint16_t queue, uint16_t size);
    void     (*set_queue_addr)(void *ctx, uint16_t queue, uint64_t desc,
                               uint64_t avail, uint64_t used);
    void     (*set_queue_enable)(void *ctx, uint16_t queue, bool enable);
    void     (*notify_queue)(void *ctx, uint16_t queue);
    bool     (*read_config)(void *ctx, uint16_t offset, void *dst,
                            uint32_t len);
} virtnet_transport_t;

/* ── Split virtqueue layouts ──────────────────────────────────── */

typedef struct {
    uint64_t addr;      /* Guest physical buffer address            */
    uint32_t len;       /* Buffer length                            */
    uint16_t flags;     /* VIRTQ_DESC_F_*                          */
    uint16_t next;      /* Next descriptor when F_NEXT is set       */
} virtq_desc_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[VIRTNET_QUEUE_SIZE];
} virtq_avail_t;

typedef struct {
    uint32_t id;        /* Head descriptor index                    */
    uint32_t len;       /* Bytes written by the device              */
} virtq_used_elem_t;

typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[VIRTNET_QUEUE_SIZE];
} virtq_used_t;

typedef struct {
    uint16_t size;              /* Active descriptor count          */
    uint16_t reserved;
    virtq_desc_t  desc[VIRTNET_QUEUE_SIZE];
    virtq_avail_t avail;
    virtq_used_t  used;
} virtnet_queue_t;

/* ── Driver instance (caller-provided, zero heap) ─────────────── */

typedef struct {
    const virtnet_transport_t *transport;       /* Opaque vtable       */
    uint64_t features;                          /* Negotiated features */
    uint8_t  mac[6];                            /* Station address     */
    uint16_t link_status;                       /* Link up flag        */
    uint16_t rx_last_used;                      /* Consumed used index */
    bool     initialized;
    uint8_t  reserved[3];
    virtnet_queue_t rxq;                        /* Receive queue 0     */
    virtnet_queue_t txq;                        /* Transmit queue 1    */
    uint8_t rx_buf[VIRTNET_QUEUE_SIZE][VIRTNET_FRAME_MAX];
    uint8_t tx_buf[VIRTNET_QUEUE_SIZE][VIRTNET_FRAME_MAX];
} virtnet_dev_t;

/* ── API ──────────────────────────────────────────────────────── */

virtnet_status_t virtnet_init(virtnet_dev_t *dev,
                              const virtnet_transport_t *transport);
virtnet_status_t virtnet_send(virtnet_dev_t *dev, const void *frame,
                              uint32_t len);
virtnet_status_t virtnet_recv(virtnet_dev_t *dev, void *buf, uint32_t cap,
                              uint32_t *out_len);
virtnet_status_t virtnet_get_mac(const virtnet_dev_t *dev,
                                 uint8_t out_mac[6]);

#endif /* VIRTIO_NET_H */
