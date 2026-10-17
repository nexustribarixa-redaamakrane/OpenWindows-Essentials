/*
 * dmactl.c - OpenWindows 8237 DMA Controller Driver (.owc)
 *
 * The 8237 exposes a shared address/count flip-flop: it must be reset
 * before each 16-bit (low, high) byte pair is written. Page registers
 * supply address bits 16-23. Channel 4 is the cascade link between the
 * two controllers and is therefore never allocatable.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "dmactl.h"

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

/* ── Controller register map ──────────────────────────────────── */

#define DMA1_STATUS_CMD    0x08u
#define DMA1_MASK_REG      0x0Au
#define DMA1_MODE_REG      0x0Bu
#define DMA1_CLEAR_FF      0x0Cu
#define DMA1_MASTER_CLEAR  0x0Du
#define DMA1_MASK_ALL      0x0Fu

#define DMA2_STATUS_CMD    0xD0u
#define DMA2_MASK_REG      0xD4u
#define DMA2_MODE_REG      0xD6u
#define DMA2_CLEAR_FF      0xD8u
#define DMA2_MASTER_CLEAR  0xDAu
#define DMA2_MASK_ALL      0xDEu

/* Per-channel base address, count and page registers. */
static const uint16_t dma_addr_port[DMA_MAX_CHANNELS] = {
    0x00u, 0x02u, 0x04u, 0x06u, 0xC0u, 0xC4u, 0xC8u, 0xCCu
};
static const uint16_t dma_count_port[DMA_MAX_CHANNELS] = {
    0x01u, 0x03u, 0x05u, 0x07u, 0xC2u, 0xC6u, 0xCAu, 0xCEu
};
static const uint16_t dma_page_port[DMA_MAX_CHANNELS] = {
    0x87u, 0x83u, 0x81u, 0x82u, 0x8Fu, 0x8Bu, 0x89u, 0x8Au
};

/* ── Internal state ───────────────────────────────────────────── */

static bool dmactl_initialized;
static uint8_t dmactl_used;     /* one bit per allocated channel */

/* ── Helpers ──────────────────────────────────────────────────── */

static bool dma_channel_valid(uint8_t channel)
{
    return channel < DMA_MAX_CHANNELS && channel != 4u;
}

static uint8_t dma_local_channel(uint8_t channel)
{
    return (uint8_t)(channel & 0x03u);
}

static uint16_t dma_status_port(uint8_t channel)
{
    return (channel < 4u) ? DMA1_STATUS_CMD : DMA2_STATUS_CMD;
}

static uint16_t dma_mask_port(uint8_t channel)
{
    return (channel < 4u) ? DMA1_MASK_REG : DMA2_MASK_REG;
}

static uint16_t dma_mode_port(uint8_t channel)
{
    return (channel < 4u) ? DMA1_MODE_REG : DMA2_MODE_REG;
}

static uint16_t dma_clear_ff_port(uint8_t channel)
{
    return (channel < 4u) ? DMA1_CLEAR_FF : DMA2_CLEAR_FF;
}

static void dma_write_mask(uint8_t channel, bool masked)
{
    uint8_t value = dma_local_channel(channel);
    if (masked) value |= 0x04u;
    ow_outb(dma_mask_port(channel), value);
}

static uint32_t dma_count_used_bits(void)
{
    uint32_t count = 0u;
    for (uint8_t ch = 0u; ch < DMA_MAX_CHANNELS; ++ch) {
        if ((dmactl_used & (uint8_t)(1u << ch)) != 0u) count++;
    }
    return count;
}

/* ── Public API ───────────────────────────────────────────────── */

bool dma_init(void)
{
    dmactl_used = 0u;

    /* Master clear resets the flip-flops on both controllers. */
    ow_outb(DMA1_MASTER_CLEAR, 0u);
    ow_outb(DMA2_MASTER_CLEAR, 0u);

    /* Mask everything until a channel is explicitly allocated. */
    ow_outb(DMA1_MASK_ALL, 0u);
    ow_outb(DMA2_MASK_ALL, 0u);

    dmactl_initialized = true;
    return true;
}

int32_t dma_alloc_channel(void)
{
    static const uint8_t order[DMA_USABLE_CHANNELS] = {
        0u, 1u, 2u, 3u, 5u, 6u, 7u
    };

    if (!dmactl_initialized) return -1;

    for (uint32_t i = 0u; i < DMA_USABLE_CHANNELS; ++i) {
        uint8_t channel = order[i];
        uint8_t bit = (uint8_t)(1u << channel);
        if ((dmactl_used & bit) == 0u) {
            dmactl_used |= bit;
            return (int32_t)channel;
        }
    }
    return -1;
}

bool dma_free_channel(uint8_t channel)
{
    if (!dmactl_initialized || !dma_channel_valid(channel)) return false;

    dma_write_mask(channel, true);
    dmactl_used &= (uint8_t)~(1u << channel);
    return true;
}

bool dma_start_transfer(uint8_t channel, uint32_t address, uint32_t count,
                        const dma_transfer_desc_t *desc)
{
    if (!dmactl_initialized || !dma_channel_valid(channel) || !desc) {
        return false;
    }
    if (count == 0u || count > DMA_TRANSFER_MAX_BYTES) return false;

    /* The 8237 reaches only the low 16 MiB through 8-bit page registers. */
    if ((uint64_t)address + (uint64_t)count > (uint64_t)DMA_ADDRESS_LIMIT) {
        return false;
    }

    /* The channel must have been allocated before it can be programmed. */
    if ((dmactl_used & (uint8_t)(1u << channel)) == 0u) return false;

    uint8_t local = dma_local_channel(channel);
    uint16_t count_value = (uint16_t)(count - 1u);

    /* Mask the channel while its registers change. */
    dma_write_mask(channel, true);

    /* Reset the shared address/count flip-flop. */
    ow_outb(dma_clear_ff_port(channel), 0u);

    uint8_t mode = (uint8_t)(((uint32_t)desc->mode & 0x03u) << 6);
    mode |= (uint8_t)(((uint32_t)desc->direction & 0x03u) << 2);
    if (desc->autoinit)  mode |= 0x10u;
    if (desc->decrement) mode |= 0x20u;
    mode |= local;
    ow_outb(dma_mode_port(channel), mode);

    /* Address low/high, then the page register for bits 16-23. */
    ow_outb(dma_addr_port[channel], (uint8_t)(address & 0xFFu));
    ow_outb(dma_addr_port[channel], (uint8_t)((address >> 8) & 0xFFu));
    ow_outb(dma_page_port[channel], (uint8_t)((address >> 16) & 0xFFu));

    /* The flip-flop auto-resets after the address pair, so the count
       low/high pair can follow directly. */
    ow_outb(dma_count_port[channel], (uint8_t)(count_value & 0xFFu));
    ow_outb(dma_count_port[channel], (uint8_t)((count_value >> 8) & 0xFFu));

    dma_write_mask(channel, false);
    return true;
}

bool dma_wait(uint8_t channel, uint32_t poll_limit)
{
    if (!dmactl_initialized || !dma_channel_valid(channel)) return false;

    uint8_t bit = (uint8_t)(1u << dma_local_channel(channel));
    if (poll_limit == 0u) poll_limit = 1000000u;

    for (uint32_t i = 0u; i < poll_limit; ++i) {
        if ((ow_inb(dma_status_port(channel)) & bit) != 0u) return true;
    }
    return false;
}

uint32_t dma_channels_in_use(void)
{
    return dma_count_used_bits();
}

bool dma_is_initialized(void)
{
    return dmactl_initialized;
}
