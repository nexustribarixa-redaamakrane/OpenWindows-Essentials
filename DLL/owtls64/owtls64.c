/*
 * owtls64.c - OpenWindows TLS/Crypto Transport Layer (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owtls64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owtls64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owtls_init(void)
{
    (void)owtls64_ready;
    /* TODO: implement owtls_init */
}
void owtls_handshake(void)
{
    (void)owtls64_ready;
    /* TODO: implement owtls_handshake */
}
void owtls_send(void)
{
    (void)owtls64_ready;
    /* TODO: implement owtls_send */
}
void owtls_recv(void)
{
    (void)owtls64_ready;
    /* TODO: implement owtls_recv */
}
void owtls_close(void)
{
    (void)owtls64_ready;
    /* TODO: implement owtls_close */
}

