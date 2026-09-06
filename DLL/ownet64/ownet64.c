/*
 * ownet64.c - OpenWindows Network Stack Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ownet64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ownet64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void ownet_init(void)
{
    (void)ownet64_ready;
    /* TODO: implement ownet_init */
}
void ownet_socket(void)
{
    (void)ownet64_ready;
    /* TODO: implement ownet_socket */
}
void ownet_bind(void)
{
    (void)ownet64_ready;
    /* TODO: implement ownet_bind */
}
void ownet_connect(void)
{
    (void)ownet64_ready;
    /* TODO: implement ownet_connect */
}
void ownet_send(void)
{
    (void)ownet64_ready;
    /* TODO: implement ownet_send */
}
void ownet_recv(void)
{
    (void)ownet64_ready;
    /* TODO: implement ownet_recv */
}
void ownet_close(void)
{
    (void)ownet64_ready;
    /* TODO: implement ownet_close */
}

