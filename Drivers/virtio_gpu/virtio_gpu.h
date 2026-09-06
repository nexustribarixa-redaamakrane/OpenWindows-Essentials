/*
 * virtio_gpu.h - OpenWindows VirtIO GPU Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void virtgpu_init(void);
void virtgpu_create_resource(void);
void virtgpu_transfer(void);
void virtgpu_flush(void);

#endif /* VIRTIO_GPU_H */

