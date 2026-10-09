/*
 * dnscli.c - OpenWindows DNS Resolver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "dnscli.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool dnscli_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void dns_init(void)
{
    if (!dnscli_initialized) { return; }
    /* TODO: implement dns_init */
    (void)0;
}
void dns_resolve(void)
{
    if (!dnscli_initialized) { return; }
    /* TODO: implement dns_resolve */
    (void)0;
}
void dns_cache_flush(void)
{
    if (!dnscli_initialized) { return; }
    /* TODO: implement dns_cache_flush */
    (void)0;
}
void dns_set_server(void)
{
    if (!dnscli_initialized) { return; }
    /* TODO: implement dns_set_server */
    (void)0;
}

