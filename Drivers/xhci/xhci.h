/*
 * xhci.h - OpenWindows Extensible Host Controller Interface (USB 3.x) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef XHCI_H
#define XHCI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void xhci_init(void);
void xhci_reset(void);
void xhci_poll_port(void);
void xhci_submit_trb(void);

#endif /* XHCI_H */

