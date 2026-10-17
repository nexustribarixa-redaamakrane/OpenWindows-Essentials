/*
 * rtl8139.c - OpenWindows Realtek RTL8139 NIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "rtl8139.h"

/* ── Capacity / timing constants ──────────────────────────────── */

#define RTL_RESET_SPIN 100000u

/* ── Port I/O helpers ─────────────────────────────────────────── */

static inline uint8_t ow_inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ow_outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t ow_inw(uint16_t port)
{
    uint16_t v;
    __asm__ __volatile__("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ow_outw(uint16_t port, uint16_t value)
{
    __asm__ __volatile__("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t ow_inl(uint16_t port)
{
    uint32_t v;
    __asm__ __volatile__("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ow_outl(uint16_t port, uint32_t value)
{
    __asm__ __volatile__("outl %0, %1" : : "a"(value), "Nd"(port));
}

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

/* Copy from the circular RX ring, honouring wraparound. */
static void rtl8139_rx_copy(const rtl8139_dev_t *dev, uint32_t offset,
                            void *dst, uint32_t len)
{
    uint8_t *d = (uint8_t *)dst;
    uint32_t i;
    for (i = 0; i < len; ++i) {
        d[i] = dev->rx_buf[(offset + i) % dev->rx_buf_len];
    }
}

/* Read the six station-address registers. */
static void rtl8139_load_mac(rtl8139_dev_t *dev)
{
    uint32_t i;
    for (i = 0; i < 6u; ++i) {
        dev->mac[i] = ow_inb((uint16_t)(dev->io_base + RTL_IDR0 + i));
    }
}

/* ── Implementation ───────────────────────────────────────────── */

rtl8139_status_t rtl8139_reset(rtl8139_dev_t *dev)
{
    uint32_t spin;

    if (dev == NULL) {
        return RTL8139_ERR_PARAM;
    }

    ow_outb((uint16_t)(dev->io_base + RTL_CR), RTL_CR_RST);
    for (spin = 0; spin < RTL_RESET_SPIN; ++spin) {
        if ((ow_inb((uint16_t)(dev->io_base + RTL_CR)) & RTL_CR_RST) == 0u) {
            break;
        }
    }
    if (spin >= RTL_RESET_SPIN) {
        return RTL8139_ERR_TIMEOUT;
    }

    /* Leave receiver/transmitter disabled; init reconfigures them. */
    ow_outb((uint16_t)(dev->io_base + RTL_CR), 0u);
    dev->rx_offset = 0u;
    return RTL8139_OK;
}

rtl8139_status_t rtl8139_init(rtl8139_dev_t *dev, uint16_t io_base,
                              uint8_t *rx_buffer, uint32_t rx_len)
{
    if (dev == NULL || rx_buffer == NULL) {
        return RTL8139_ERR_PARAM;
    }
    if (rx_len < RTL8139_RX_MIN) {
        return RTL8139_ERR_PARAM;
    }

    ow_memset(dev, 0, sizeof(*dev));
    dev->io_base = io_base;
    dev->rx_buf = rx_buffer;
    dev->rx_buf_len = rx_len;

    if (rtl8139_reset(dev) != RTL8139_OK) {
        return RTL8139_ERR_TIMEOUT;
    }

    rtl8139_load_mac(dev);

    /* Point the controller at the caller-provided RX ring. */
    ow_outl((uint16_t)(io_base + RTL_RBSTART),
            (uint32_t)(uintptr_t)rx_buffer);

    /* Disable interrupts, clear any pending status. */
    ow_outw((uint16_t)(io_base + RTL_IMR), 0x0000u);
    ow_outw((uint16_t)(io_base + RTL_ISR), 0xFFFFu);

    /* Accept broadcast + physical/multicast, unlimited DMA burst. */
    ow_outl((uint16_t)(io_base + RTL_RCR),
            RTL_RCR_AAP | RTL_RCR_APM | RTL_RCR_AM | RTL_RCR_AB |
            RTL_RCR_WRAP | RTL_RCR_MXDMA);
    ow_outl((uint16_t)(io_base + RTL_TCR), RTL_TCR_MXDMA);

    /* Reset the read pointer to the start of the ring. */
    dev->rx_offset = 0u;
    ow_outw((uint16_t)(io_base + RTL_CAPR), 0x0000u);

    ow_outb((uint16_t)(io_base + RTL_CR), RTL_CR_RE | RTL_CR_TE);

    dev->tx_slot = 0u;
    dev->initialized = true;
    return RTL8139_OK;
}

rtl8139_status_t rtl8139_send(rtl8139_dev_t *dev, const void *frame,
                              uint32_t len)
{
    uint32_t slot;
    uint32_t tsd;

    if (dev == NULL || frame == NULL) {
        return RTL8139_ERR_PARAM;
    }
    if (!dev->initialized) {
        return RTL8139_ERR_NOT_READY;
    }
    if (len == 0u || len > RTL8139_FRAME_MAX) {
        return RTL8139_ERR_PARAM;
    }

    slot = dev->tx_slot & (RTL8139_TX_SLOTS - 1u);
    tsd = ow_inl((uint16_t)(dev->io_base + RTL_TSD0 + (slot * 4u)));
    if ((tsd & RTL_TSD_OWN) != 0u) {
        return RTL8139_ERR_NO_SPACE;   /* previous frame still in flight */
    }

    ow_memcpy(dev->tx_buf[slot], frame, len);
    ow_outl((uint16_t)(dev->io_base + RTL_TSAD0 + (slot * 4u)),
            (uint32_t)(uintptr_t)dev->tx_buf[slot]);
    /* Writing the length clears OWN and kicks off the transfer. */
    ow_outl((uint16_t)(dev->io_base + RTL_TSD0 + (slot * 4u)),
            len & RTL_TSD_SIZE_MASK);

    dev->tx_slot = (uint8_t)((slot + 1u) & (RTL8139_TX_SLOTS - 1u));
    return RTL8139_OK;
}

rtl8139_status_t rtl8139_recv(rtl8139_dev_t *dev, void *buf, uint32_t cap,
                              uint32_t *out_len)
{
    uint16_t capr;
    uint16_t cbr;
    uint8_t  header[4];
    uint16_t status;
    uint32_t pkt_len;
    uint32_t payload_len;
    uint32_t next;

    if (dev == NULL || buf == NULL || out_len == NULL) {
        return RTL8139_ERR_PARAM;
    }
    if (!dev->initialized) {
        return RTL8139_ERR_NOT_READY;
    }
    if (dev->rx_buf == NULL || dev->rx_buf_len == 0u) {
        return RTL8139_ERR_NOT_READY;
    }

    capr = (uint16_t)dev->rx_offset;
    cbr  = ow_inw((uint16_t)(dev->io_base + RTL_CBR));
    if (cbr == capr) {
        return RTL8139_ERR_NOT_READY;   /* nothing received yet */
    }

    /* The 4-byte RX header precedes every packet. */
    rtl8139_rx_copy(dev, (uint32_t)capr, header, 4u);
    status  = (uint16_t)(header[0] | ((uint16_t)header[1] << 8));
    pkt_len = (uint32_t)(header[2] | ((uint16_t)header[3] << 8));

    if (pkt_len < 4u) {
        return RTL8139_ERR_IO;
    }
    payload_len = pkt_len - 4u;   /* trailing CRC is included in length */

    if ((status & RTL_RSR_ROK) == 0u) {
        return RTL8139_ERR_IO;
    }
    if (payload_len > cap) {
        return RTL8139_ERR_NO_SPACE;
    }

    rtl8139_rx_copy(dev, (uint32_t)capr + 4u, buf, payload_len);

    /* Advance CAPR past header + data, dword-aligned, wrapping the ring. */
    next = (uint32_t)capr + pkt_len + 4u;
    next = (next + 3u) & ~3u;
    if (next >= dev->rx_buf_len) {
        next -= dev->rx_buf_len;
    }
    dev->rx_offset = next;
    ow_outw((uint16_t)(dev->io_base + RTL_CAPR), (uint16_t)next);

    *out_len = payload_len;
    return RTL8139_OK;
}

rtl8139_status_t rtl8139_get_mac(const rtl8139_dev_t *dev, uint8_t out_mac[6])
{
    uint32_t i;

    if (dev == NULL || out_mac == NULL) {
        return RTL8139_ERR_PARAM;
    }
    if (!dev->initialized) {
        return RTL8139_ERR_NOT_READY;
    }
    for (i = 0; i < 6u; ++i) {
        out_mac[i] = dev->mac[i];
    }
    return RTL8139_OK;
}
