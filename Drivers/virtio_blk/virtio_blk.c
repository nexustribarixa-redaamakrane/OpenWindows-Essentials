/*
 * virtio_blk.c - OpenWindows VirtIO Block Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "virtio_blk.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool virtio_blk_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void virtblk_init(void)
{
    if (!virtio_blk_initialized) { return; }
    /* TODO: implement virtblk_init */
    (void)0;
}
void virtblk_read(void)
{
    if (!virtio_blk_initialized) { return; }
    /* TODO: implement virtblk_read */
    (void)0;
}
void virtblk_write(void)
{
    if (!virtio_blk_initialized) { return; }
    /* TODO: implement virtblk_write */
    (void)0;
}
void virtblk_flush(void)
{
    if (!virtio_blk_initialized) { return; }
    /* TODO: implement virtblk_flush */
    (void)0;
}

