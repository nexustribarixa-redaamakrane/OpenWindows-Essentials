/*
 * ehci.h - OpenWindows Enhanced Host Controller Interface (USB 2.0) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef EHCI_H
#define EHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void ehci_init(void);
void ehci_reset(void);
void ehci_poll_port(void);
void ehci_submit_urb(void);

#endif /* EHCI_H */

