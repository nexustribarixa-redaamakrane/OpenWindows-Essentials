/*
 * owacl64.c - OpenWindows Access Control List Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owacl64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owacl64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owacl_init(void)
{
    (void)owacl64_ready;
    /* TODO: implement owacl_init */
}
void owacl_check(void)
{
    (void)owacl64_ready;
    /* TODO: implement owacl_check */
}
void owacl_grant(void)
{
    (void)owacl64_ready;
    /* TODO: implement owacl_grant */
}
void owacl_revoke(void)
{
    (void)owacl64_ready;
    /* TODO: implement owacl_revoke */
}
void owacl_enumerate(void)
{
    (void)owacl64_ready;
    /* TODO: implement owacl_enumerate */
}

