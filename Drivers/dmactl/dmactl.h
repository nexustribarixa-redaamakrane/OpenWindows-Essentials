/*
 * dmactl.h - OpenWindows DMA Controller Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef DMACTL_H
#define DMACTL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void dma_init(void);
void dma_alloc_channel(void);
void dma_start_transfer(void);
void dma_wait(void);

#endif /* DMACTL_H */

