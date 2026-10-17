/*
 * dmactl.h - OpenWindows 8237 DMA Controller Driver (.owc)
 *
 * Programs the two cascaded 8237 DMA controllers (master ports
 * 0x00-0x0F, slave ports 0xC0-0xDE with page registers 0x81-0x8F).
 * Channels are allocated from a file-scope bitmap; transfers program
 * base address, count, page and mode before unmasking the channel.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef DMACTL_H
#define DMACTL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define DMA_MAX_CHANNELS        8u      /* 8237 channel space         */
#define DMA_USABLE_CHANNELS     7u      /* channel 4 is the cascade   */
#define DMA_TRANSFER_MAX_BYTES  0x10000u /* 64 KiB span per program   */
#define DMA_ADDRESS_LIMIT       0x1000000u /* 16 MiB via 8-bit pages  */

/* Transfer direction field (mode register bits 2-3). */
typedef enum {
    DMA_DIR_VERIFY = 0u,   /* pseudo transfer, no data moved          */
    DMA_DIR_WRITE  = 1u,   /* memory -> device                        */
    DMA_DIR_READ   = 2u    /* device -> memory                        */
} dma_direction_t;

/* Transfer mode field (mode register bits 6-7). */
typedef enum {
    DMA_MODE_DEMAND  = 0u,
    DMA_MODE_SINGLE  = 1u,
    DMA_MODE_BLOCK   = 2u,
    DMA_MODE_CASCADE = 3u
} dma_mode_t;

/* Full description of one channel programming operation. */
typedef struct {
    dma_direction_t direction;  /* transfer direction                  */
    bool            autoinit;   /* restore address/count at terminal count */
    bool            decrement;  /* count downward instead of upward    */
    uint8_t         mode;       /* dma_mode_t                          */
} dma_transfer_desc_t;

/* Reset both controllers and mask every channel. */
bool dma_init(void);

/* Allocate the lowest free usable channel, or -1 when none remain. */
int32_t dma_alloc_channel(void);

/* Release a channel and mask it. */
bool dma_free_channel(uint8_t channel);

/* Program and start a transfer on an allocated channel. */
bool dma_start_transfer(uint8_t channel, uint32_t address, uint32_t count,
                        const dma_transfer_desc_t *desc);

/* Poll the controller status register for terminal count. */
bool dma_wait(uint8_t channel, uint32_t poll_limit);

/* Number of channels currently allocated. */
uint32_t dma_channels_in_use(void);

/* True after a successful dma_init(). */
bool dma_is_initialized(void);

#endif /* DMACTL_H */
