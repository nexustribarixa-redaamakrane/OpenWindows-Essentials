/*
 * owio64.c - OpenWindows Port I/O Abstraction Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owio64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owio64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owio_inb(void)
{
    (void)owio64_ready;
    /* TODO: implement owio_inb */
}
void owio_outb(void)
{
    (void)owio64_ready;
    /* TODO: implement owio_outb */
}
void owio_inw(void)
{
    (void)owio64_ready;
    /* TODO: implement owio_inw */
}
void owio_outw(void)
{
    (void)owio64_ready;
    /* TODO: implement owio_outw */
}
void owio_ind(void)
{
    (void)owio64_ready;
    /* TODO: implement owio_ind */
}
void owio_outd(void)
{
    (void)owio64_ready;
    /* TODO: implement owio_outd */
}
void owio_wait(void)
{
    (void)owio64_ready;
    /* TODO: implement owio_wait */
}

