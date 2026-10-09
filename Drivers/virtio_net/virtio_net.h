/*
 * virtio_net.h - OpenWindows VirtIO Network Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void virtnet_init(void);
void virtnet_send(void);
void virtnet_recv(void);
void virtnet_get_mac(void);

#endif /* VIRTIO_NET_H */

