/*
 * ipstack.c - OpenWindows Minimal IP Stack (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ipstack.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ipstack_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void ip_init(void)
{
    if (!ipstack_initialized) { return; }
    /* TODO: implement ip_init */
    (void)0;
}
void ip_send(void)
{
    if (!ipstack_initialized) { return; }
    /* TODO: implement ip_send */
    (void)0;
}
void ip_recv(void)
{
    if (!ipstack_initialized) { return; }
    /* TODO: implement ip_recv */
    (void)0;
}
void ip_route(void)
{
    if (!ipstack_initialized) { return; }
    /* TODO: implement ip_route */
    (void)0;
}
void ip_arp_resolve(void)
{
    if (!ipstack_initialized) { return; }
    /* TODO: implement ip_arp_resolve */
    (void)0;
}

