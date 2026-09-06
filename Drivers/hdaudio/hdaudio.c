/*
 * hdaudio.c - OpenWindows Intel HD Audio Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "hdaudio.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool hdaudio_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void hda_init(void)
{
    if (!hdaudio_initialized) { return; }
    /* TODO: implement hda_init */
    (void)0;
}
void hda_reset(void)
{
    if (!hdaudio_initialized) { return; }
    /* TODO: implement hda_reset */
    (void)0;
}
void hda_play_stream(void)
{
    if (!hdaudio_initialized) { return; }
    /* TODO: implement hda_play_stream */
    (void)0;
}
void hda_stop_stream(void)
{
    if (!hdaudio_initialized) { return; }
    /* TODO: implement hda_stop_stream */
    (void)0;
}
void hda_set_volume(void)
{
    if (!hdaudio_initialized) { return; }
    /* TODO: implement hda_set_volume */
    (void)0;
}

