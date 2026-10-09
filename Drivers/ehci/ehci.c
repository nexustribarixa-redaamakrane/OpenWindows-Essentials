/*
 * ehci.c - OpenWindows Enhanced Host Controller Interface (USB 2.0) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ehci.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ehci_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void ehci_init(void)
{
    if (!ehci_initialized) { return; }
    /* TODO: implement ehci_init */
    (void)0;
}
void ehci_reset(void)
{
    if (!ehci_initialized) { return; }
    /* TODO: implement ehci_reset */
    (void)0;
}
void ehci_poll_port(void)
{
    if (!ehci_initialized) { return; }
    /* TODO: implement ehci_poll_port */
    (void)0;
}
void ehci_submit_urb(void)
{
    if (!ehci_initialized) { return; }
    /* TODO: implement ehci_submit_urb */
    (void)0;
}

