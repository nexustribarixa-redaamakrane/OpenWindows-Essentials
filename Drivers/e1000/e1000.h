/*
 * e1000.h - OpenWindows Intel e1000 NIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef E1000_H
#define E1000_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void e1000_init(void);
void e1000_send(void);
void e1000_recv(void);
void e1000_get_mac(void);

#endif /* E1000_H */

