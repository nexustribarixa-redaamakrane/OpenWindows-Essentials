/*
 * owsock64.c - OpenWindows Socket Abstraction Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owsock64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owsock64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owsock_create(void)
{
    (void)owsock64_ready;
    /* TODO: implement owsock_create */
}
void owsock_bind(void)
{
    (void)owsock64_ready;
    /* TODO: implement owsock_bind */
}
void owsock_listen(void)
{
    (void)owsock64_ready;
    /* TODO: implement owsock_listen */
}
void owsock_accept(void)
{
    (void)owsock64_ready;
    /* TODO: implement owsock_accept */
}
void owsock_connect(void)
{
    (void)owsock64_ready;
    /* TODO: implement owsock_connect */
}
void owsock_send(void)
{
    (void)owsock64_ready;
    /* TODO: implement owsock_send */
}
void owsock_recv(void)
{
    (void)owsock64_ready;
    /* TODO: implement owsock_recv */
}
void owsock_close(void)
{
    (void)owsock64_ready;
    /* TODO: implement owsock_close */
}

