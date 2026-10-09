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
void virtblk_init(void);
void virtblk_read(void);
void virtblk_write(void);
void virtblk_flush(void);

#endif /* VIRTIO_BLK_H */

