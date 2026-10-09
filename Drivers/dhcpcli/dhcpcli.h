/*
 * dhcpcli.h - OpenWindows DHCP Client (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef DHCPCLI_H
#define DHCPCLI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void dhcp_init(void);
void dhcp_discover(void);
void dhcp_request(void);
void dhcp_release(void);

#endif /* DHCPCLI_H */

