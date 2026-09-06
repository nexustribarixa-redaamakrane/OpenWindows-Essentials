/*
 * ipstack.h - OpenWindows Minimal IP Stack (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef IPSTACK_H
#define IPSTACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void ip_init(void);
void ip_send(void);
void ip_recv(void);
void ip_route(void);
void ip_arp_resolve(void);

#endif /* IPSTACK_H */

