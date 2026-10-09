/*
 * nvmedrv.h - OpenWindows NVMe Storage Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef NVMEDRV_H
#define NVMEDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void nvme_init(void);
void nvme_identify(void);
void nvme_read(void);
void nvme_write(void);
void nvme_flush(void);

#endif /* NVMEDRV_H */

