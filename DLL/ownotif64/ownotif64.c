/*
 * ownotif64.c - OpenWindows Notification Service Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ownotif64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool ownotif64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void ownotif_init(void)
{
    (void)ownotif64_ready;
    /* TODO: implement ownotif_init */
}
void ownotif_send(void)
{
    (void)ownotif64_ready;
    /* TODO: implement ownotif_send */
}
void ownotif_dismiss(void)
{
    (void)ownotif64_ready;
    /* TODO: implement ownotif_dismiss */
}
void ownotif_register(void)
{
    (void)ownotif64_ready;
    /* TODO: implement ownotif_register */
}
void ownotif_get_pending(void)
{
    (void)ownotif64_ready;
    /* TODO: implement ownotif_get_pending */
}

