/*
 * virtio_net.c - OpenWindows VirtIO Network Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "virtio_net.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool virtio_net_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void virtnet_init(void)
{
    if (!virtio_net_initialized) { return; }
    /* TODO: implement virtnet_init */
    (void)0;
}
void virtnet_send(void)
{
    if (!virtio_net_initialized) { return; }
    /* TODO: implement virtnet_send */
    (void)0;
}
void virtnet_recv(void)
{
    if (!virtio_net_initialized) { return; }
    /* TODO: implement virtnet_recv */
    (void)0;
}
void virtnet_get_mac(void)
{
    if (!virtio_net_initialized) { return; }
    /* TODO: implement virtnet_get_mac */
    (void)0;
}

