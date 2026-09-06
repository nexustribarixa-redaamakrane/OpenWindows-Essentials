/*
 * owaudio64.c - OpenWindows Audio Mixer Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owaudio64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owaudio64_ready = false;

/* ── Stub implementations ─────────────────────────────────────── */
void owaudio_init(void)
{
    (void)owaudio64_ready;
    /* TODO: implement owaudio_init */
}
void owaudio_play(void)
{
    (void)owaudio64_ready;
    /* TODO: implement owaudio_play */
}
void owaudio_stop(void)
{
    (void)owaudio64_ready;
    /* TODO: implement owaudio_stop */
}
void owaudio_set_volume(void)
{
    (void)owaudio64_ready;
    /* TODO: implement owaudio_set_volume */
}
void owaudio_get_devices(void)
{
    (void)owaudio64_ready;
    /* TODO: implement owaudio_get_devices */
}

