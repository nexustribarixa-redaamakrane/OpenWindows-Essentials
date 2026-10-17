/*
 * e1000.c - OpenWindows Intel e1000 NIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "e1000.h"

/* ── Capacity / timing constants ──────────────────────────────── */

#define E1000_RESET_SPIN   100000u
#define E1000_TIPG_DEFAULT 0x0060200Au

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

/* ── MMIO access ──────────────────────────────────────────────── */

static uint32_t e1000_reg_read(const e1000_dev_t *dev, uint32_t reg)
{
    return *(volatile uint32_t *)(dev->mmio + reg);
}

static void e1000_reg_write(const e1000_dev_t *dev, uint32_t reg,
                            uint32_t value)
{
    *(volatile uint32_t *)(dev->mmio + reg) = value;
}

/* Load the receive address registers into the software MAC field. */
static void e1000_load_mac(e1000_dev_t *dev)
{
    uint32_t ral = e1000_reg_read(dev, E1000_REG_RAL);
    uint32_t rah = e1000_reg_read(dev, E1000_REG_RAH);

    dev->mac[0] = (uint8_t)(ral & 0xFFu);
    dev->mac[1] = (uint8_t)((ral >> 8) & 0xFFu);
    dev->mac[2] = (uint8_t)((ral >> 16) & 0xFFu);
    dev->mac[3] = (uint8_t)((ral >> 24) & 0xFFu);
    dev->mac[4] = (uint8_t)(rah & 0xFFu);
    dev->mac[5] = (uint8_t)((rah >> 8) & 0xFFu);
}

/* Program the receive descriptor ring. */
static e1000_status_t e1000_setup_rx(e1000_dev_t *dev)
{
    uint32_t i;
    uint64_t base = (uint64_t)(uintptr_t)dev->rx_ring;

    ow_memset(dev->rx_ring, 0, sizeof(dev->rx_ring));
    for (i = 0; i < E1000_RX_RING_SIZE; ++i) {
        dev->rx_ring[i].addr   = (uint64_t)(uintptr_t)dev->rx_buf[i];
        dev->rx_ring[i].status = 0u;
    }

    e1000_reg_write(dev, E1000_REG_RDBAL, (uint32_t)(base & 0xFFFFFFFFu));
    e1000_reg_write(dev, E1000_REG_RDBAH, (uint32_t)(base >> 32));
    e1000_reg_write(dev, E1000_REG_RDLEN, (uint32_t)sizeof(dev->rx_ring));
    e1000_reg_write(dev, E1000_REG_RDH, 0u);
    e1000_reg_write(dev, E1000_REG_RDT, E1000_RX_RING_SIZE - 1u);

    dev->rx_tail = 0u;

    /* Accept broadcast/unicast, strip CRC, 2048-byte buffers (BSIZE = 0). */
    e1000_reg_write(dev, E1000_REG_RCTL,
                    E1000_RCTL_EN | E1000_RCTL_UPE | E1000_RCTL_MPE |
                    E1000_RCTL_BAM | E1000_RCTL_SECRC);
    return E1000_OK;
}

/* Program the transmit descriptor ring. */
static e1000_status_t e1000_setup_tx(e1000_dev_t *dev)
{
    uint64_t base = (uint64_t)(uintptr_t)dev->tx_ring;

    ow_memset(dev->tx_ring, 0, sizeof(dev->tx_ring));

    e1000_reg_write(dev, E1000_REG_TDBAL, (uint32_t)(base & 0xFFFFFFFFu));
    e1000_reg_write(dev, E1000_REG_TDBAH, (uint32_t)(base >> 32));
    e1000_reg_write(dev, E1000_REG_TDLEN, (uint32_t)sizeof(dev->tx_ring));
    e1000_reg_write(dev, E1000_REG_TDH, 0u);
    e1000_reg_write(dev, E1000_REG_TDT, 0u);

    dev->tx_tail = 0u;

    e1000_reg_write(dev, E1000_REG_TCTL,
                    E1000_TCTL_EN | E1000_TCTL_PSP |
                    E1000_TCTL_CT | E1000_TCTL_COLD);
    e1000_reg_write(dev, E1000_REG_TIPG, E1000_TIPG_DEFAULT);
    return E1000_OK;
}

/* ── Implementation ───────────────────────────────────────────── */

e1000_status_t e1000_init(e1000_dev_t *dev, volatile uint8_t *mmio)
{
    uint32_t spin;

    if (dev == NULL || mmio == NULL) {
        return E1000_ERR_PARAM;
    }

    ow_memset(dev, 0, sizeof(*dev));
    dev->mmio = mmio;

    /* Mask all interrupts while we reconfigure the controller. */
    e1000_reg_write(dev, E1000_REG_IMC, 0xFFFFFFFFu);

    /* Issue a device reset and wait for the self-clearing RST bit. */
    e1000_reg_write(dev, E1000_REG_CTRL, E1000_CTRL_RST);
    for (spin = 0; spin < E1000_RESET_SPIN; ++spin) {
        if ((e1000_reg_read(dev, E1000_REG_CTRL) & E1000_CTRL_RST) == 0u) {
            break;
        }
    }
    if (spin >= E1000_RESET_SPIN) {
        return E1000_ERR_TIMEOUT;
    }

    /* Bring the link up and clear the multicast table array (128 entries). */
    e1000_reg_write(dev, E1000_REG_CTRL, E1000_CTRL_SLU);
    for (spin = 0; spin < 128u; spin += 4u) {
        e1000_reg_write(dev, E1000_REG_MTA + spin, 0u);
    }

    e1000_load_mac(dev);

    if (e1000_setup_tx(dev) != E1000_OK) {
        return E1000_ERR_IO;
    }
    if (e1000_setup_rx(dev) != E1000_OK) {
        return E1000_ERR_IO;
    }

    e1000_reg_write(dev, E1000_REG_ICR, 0xFFFFFFFFu);
    dev->initialized = true;
    return E1000_OK;
}

e1000_status_t e1000_send(e1000_dev_t *dev, const void *frame, uint32_t len)
{
    e1000_tx_desc_t *desc;
    uint32_t head;
    uint32_t next;

    if (dev == NULL || frame == NULL) {
        return E1000_ERR_PARAM;
    }
    if (!dev->initialized) {
        return E1000_ERR_NOT_READY;
    }
    if (len == 0u || len > E1000_FRAME_MAX) {
        return E1000_ERR_PARAM;
    }

    head = e1000_reg_read(dev, E1000_REG_TDH);
    next = (dev->tx_tail + 1u) % E1000_TX_RING_SIZE;
    if (next == head) {
        return E1000_ERR_NO_SPACE;   /* ring full, wait for hardware */
    }

    desc = &dev->tx_ring[dev->tx_tail];
    ow_memcpy(dev->tx_buf[dev->tx_tail], frame, len);

    desc->addr    = (uint64_t)(uintptr_t)dev->tx_buf[dev->tx_tail];
    desc->length  = (uint16_t)len;
    desc->cso     = 0u;
    desc->cmd     = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS |
                    E1000_TXD_CMD_RS;
    desc->status  = 0u;
    desc->css     = 0u;
    desc->special = 0u;

    dev->tx_tail = next;
    e1000_reg_write(dev, E1000_REG_TDT, next);
    return E1000_OK;
}

e1000_status_t e1000_recv(e1000_dev_t *dev, void *buf, uint32_t cap,
                          uint32_t *out_len)
{
    e1000_rx_desc_t *desc;
    uint32_t idx;
    uint32_t len;

    if (dev == NULL || buf == NULL || out_len == NULL) {
        return E1000_ERR_PARAM;
    }
    if (!dev->initialized) {
        return E1000_ERR_NOT_READY;
    }

    idx = dev->rx_tail;
    desc = &dev->rx_ring[idx];
    if ((desc->status & E1000_RXD_STAT_DD) == 0u) {
        return E1000_ERR_NOT_READY;  /* no packet waiting */
    }
    if (desc->errors != 0u) {
        desc->status = 0u;
        dev->rx_tail = (idx + 1u) % E1000_RX_RING_SIZE;
        e1000_reg_write(dev, E1000_REG_RDT, dev->rx_tail);
        return E1000_ERR_IO;
    }

    len = desc->length;
    if (len > cap) {
        return E1000_ERR_NO_SPACE;   /* caller must retry larger */
    }

    ow_memcpy(buf, dev->rx_buf[idx], len);
    *out_len = len;

    desc->status = 0u;
    dev->rx_tail = (idx + 1u) % E1000_RX_RING_SIZE;
    e1000_reg_write(dev, E1000_REG_RDT, dev->rx_tail);
    return E1000_OK;
}

e1000_status_t e1000_get_mac(const e1000_dev_t *dev, uint8_t out_mac[6])
{
    uint32_t i;

    if (dev == NULL || out_mac == NULL) {
        return E1000_ERR_PARAM;
    }
    if (!dev->initialized) {
        return E1000_ERR_NOT_READY;
    }
    for (i = 0; i < 6u; ++i) {
        out_mac[i] = dev->mac[i];
    }
    return E1000_OK;
}
