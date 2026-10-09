/*
 * rtl8139.h - OpenWindows Realtek RTL8139 NIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef RTL8139_H
#define RTL8139_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void rtl8139_init(void);
void rtl8139_send(void);
void rtl8139_recv(void);
void rtl8139_reset(void);

#endif /* RTL8139_H */

