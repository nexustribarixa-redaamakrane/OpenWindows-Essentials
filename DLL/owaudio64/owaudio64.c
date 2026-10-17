/*
 * owaudio64.c - OpenWindows Audio Mixer Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owaudio64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owaudio64_ready = false;
static owaudio_output_fn owaudio64_hook = NULL;
static void *owaudio64_hook_ctx = NULL;

static owaudio_channel_t owaudio64_channels[OWAUDIO_MAX_CHANNELS];
static owaudio_device_t owaudio64_devices[OWAUDIO_MAX_DEVICES];
static uint32_t owaudio64_device_count = 0u;
static uint32_t owaudio64_next_device_id = 1u;
static uint32_t owaudio64_master_volume = OWAUDIO_VOLUME_MAX;
static bool owaudio64_master_muted = false;

/* ── Internal helpers ─────────────────────────────────────────── */

static void owaudio_copy_string(char *dst, const char *src)
{
    uint32_t i = 0u;

    if (src) {
        for (i = 0u; i + 1u < OWAUDIO_NAME_MAX && src[i] != '\0'; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

static uint32_t owaudio_clamp_volume(uint32_t volume)
{
    if (volume > OWAUDIO_VOLUME_MAX) return OWAUDIO_VOLUME_MAX;
    return volume;
}

static owaudio_channel_t *owaudio_channel(uint32_t channel)
{
    if (channel >= OWAUDIO_MAX_CHANNELS) return NULL;
    return &owaudio64_channels[channel];
}

/* ── Implementation ──────────────────────────────────────────── */

bool owaudio_init(owaudio_output_fn hook, void *ctx)
{
    for (uint32_t i = 0u; i < OWAUDIO_MAX_CHANNELS; ++i) {
        owaudio64_channels[i].id = i;
        owaudio64_channels[i].volume = OWAUDIO_VOLUME_MAX;
        owaudio64_channels[i].sample_rate = 0u;
        owaudio64_channels[i].bytes_played = 0u;
        owaudio64_channels[i].used = false;
        owaudio64_channels[i].playing = false;
        owaudio64_channels[i].muted = false;
    }

    for (uint32_t i = 0u; i < OWAUDIO_MAX_DEVICES; ++i) {
        owaudio64_devices[i].id = 0u;
        owaudio64_devices[i].used = false;
        owaudio64_devices[i].volume = OWAUDIO_VOLUME_MAX;
        owaudio64_devices[i].muted = false;
        owaudio64_devices[i].name[0] = '\0';
    }

    owaudio64_hook = hook;
    owaudio64_hook_ctx = ctx;
    owaudio64_device_count = 0u;
    owaudio64_next_device_id = 1u;
    owaudio64_master_volume = OWAUDIO_VOLUME_MAX;
    owaudio64_master_muted = false;
    owaudio64_ready = true;

    /* Every mixer starts with one default output. */
    uint32_t ignored_id = 0u;
    (void)owaudio_register_device("primary", &ignored_id);

    return owaudio64_ready;
}

int32_t owaudio_play(uint32_t channel, const void *samples, size_t bytes,
                     uint32_t sample_rate)
{
    if (!owaudio64_ready) return -1;
    if (bytes != 0u && !samples) return -1;

    owaudio_channel_t *ch = owaudio_channel(channel);
    if (!ch) return -1;

    if (owaudio64_hook) {
        bool accepted = owaudio64_hook(owaudio64_hook_ctx, channel,
                                       samples, bytes, sample_rate);
        if (!accepted) return -1;
    }

    ch->used = true;
    ch->playing = true;
    ch->sample_rate = sample_rate;
    ch->bytes_played += (uint64_t)bytes;
    return (int32_t)channel;
}

bool owaudio_stop(uint32_t channel)
{
    owaudio_channel_t *ch = owaudio_channel(channel);
    if (!owaudio64_ready || !ch) return false;

    ch->playing = false;
    return true;
}

bool owaudio_set_volume(uint32_t channel, uint32_t volume)
{
    if (!owaudio64_ready) return false;

    uint32_t clamped = owaudio_clamp_volume(volume);

    if (channel == OWAUDIO_MASTER_CHANNEL) {
        owaudio64_master_volume = clamped;
        return true;
    }

    owaudio_channel_t *ch = owaudio_channel(channel);
    if (!ch) return false;

    ch->volume = clamped;
    return true;
}

bool owaudio_set_mute(uint32_t channel, bool muted)
{
    if (!owaudio64_ready) return false;

    if (channel == OWAUDIO_MASTER_CHANNEL) {
        owaudio64_master_muted = muted;
        return true;
    }

    owaudio_channel_t *ch = owaudio_channel(channel);
    if (!ch) return false;

    ch->muted = muted;
    return true;
}

uint32_t owaudio_get_devices(owaudio_device_t *out, uint32_t cap)
{
    if (!out || cap == 0u) return 0u;

    uint32_t copied = 0u;
    for (uint32_t i = 0u; i < OWAUDIO_MAX_DEVICES && copied < cap; ++i) {
        if (!owaudio64_devices[i].used) continue;
        out[copied] = owaudio64_devices[i];
        ++copied;
    }
    return copied;
}

uint32_t owaudio_get_channels(owaudio_channel_t *out, uint32_t cap)
{
    if (!out || cap == 0u) return 0u;

    uint32_t copied = 0u;
    for (uint32_t i = 0u; i < OWAUDIO_MAX_CHANNELS && copied < cap; ++i) {
        if (!owaudio64_channels[i].used) continue;
        out[copied] = owaudio64_channels[i];
        ++copied;
    }
    return copied;
}

bool owaudio_register_device(const char *name, uint32_t *out_id)
{
    if (!owaudio64_ready) return false;
    if (owaudio64_device_count >= OWAUDIO_MAX_DEVICES) return false;

    for (uint32_t i = 0u; i < OWAUDIO_MAX_DEVICES; ++i) {
        if (owaudio64_devices[i].used) continue;

        owaudio64_devices[i].id = owaudio64_next_device_id++;
        if (owaudio64_next_device_id == 0u) owaudio64_next_device_id = 1u;
        owaudio64_devices[i].volume = OWAUDIO_VOLUME_MAX;
        owaudio64_devices[i].muted = false;
        owaudio64_devices[i].used = true;
        owaudio_copy_string(owaudio64_devices[i].name, name);
        ++owaudio64_device_count;

        if (out_id) *out_id = owaudio64_devices[i].id;
        return true;
    }

    return false;
}

uint32_t owaudio_get_master_volume(void)
{
    return owaudio64_master_volume;
}

bool owaudio_is_master_muted(void)
{
    return owaudio64_master_muted;
}
