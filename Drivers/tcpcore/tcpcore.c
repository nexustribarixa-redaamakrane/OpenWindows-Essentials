/*
 * tcpcore.c - OpenWindows TCP Transport Core (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "tcpcore.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool tcpcore_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void tcp_init(void)
{
    if (!tcpcore_initialized) { return; }
    /* TODO: implement tcp_init */
    (void)0;
}
void tcp_connect(void)
{
    if (!tcpcore_initialized) { return; }
    /* TODO: implement tcp_connect */
    (void)0;
}
void tcp_listen(void)
{
    if (!tcpcore_initialized) { return; }
    /* TODO: implement tcp_listen */
    (void)0;
}
void tcp_send(void)
{
    if (!tcpcore_initialized) { return; }
    /* TODO: implement tcp_send */
    (void)0;
}
void tcp_recv(void)
{
    if (!tcpcore_initialized) { return; }
    /* TODO: implement tcp_recv */
    (void)0;
}
void tcp_close(void)
{
    if (!tcpcore_initialized) { return; }
    /* TODO: implement tcp_close */
    (void)0;
}

