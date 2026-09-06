/*
 * rtl8139.c - OpenWindows Realtek RTL8139 NIC Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "rtl8139.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool rtl8139_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void rtl8139_init(void)
{
    if (!rtl8139_initialized) { return; }
    /* TODO: implement rtl8139_init */
    (void)0;
}
void rtl8139_send(void)
{
    if (!rtl8139_initialized) { return; }
    /* TODO: implement rtl8139_send */
    (void)0;
}
void rtl8139_recv(void)
{
    if (!rtl8139_initialized) { return; }
    /* TODO: implement rtl8139_recv */
    (void)0;
}
void rtl8139_reset(void)
{
    if (!rtl8139_initialized) { return; }
    /* TODO: implement rtl8139_reset */
    (void)0;
}

