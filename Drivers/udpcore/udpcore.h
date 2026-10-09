/*
 * udpcore.h - OpenWindows UDP Transport Core (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef UDPCORE_H
#define UDPCORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void udp_init(void);
void udp_bind(void);
void udp_send(void);
void udp_recv(void);
void udp_close(void);

#endif /* UDPCORE_H */

