/*
 * virtio_gpu.c - OpenWindows VirtIO GPU Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "virtio_gpu.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool virtio_gpu_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void virtgpu_init(void)
{
    if (!virtio_gpu_initialized) { return; }
    /* TODO: implement virtgpu_init */
    (void)0;
}
void virtgpu_create_resource(void)
{
    if (!virtio_gpu_initialized) { return; }
    /* TODO: implement virtgpu_create_resource */
    (void)0;
}
void virtgpu_transfer(void)
{
    if (!virtio_gpu_initialized) { return; }
    /* TODO: implement virtgpu_transfer */
    (void)0;
}
void virtgpu_flush(void)
{
    if (!virtio_gpu_initialized) { return; }
    /* TODO: implement virtgpu_flush */
    (void)0;
}

