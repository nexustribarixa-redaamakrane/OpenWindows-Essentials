/*
 * owaudio64.h - OpenWindows Audio Mixer Library (.owd)
 *
 * Fixed-size mixer with per-channel volume, a master volume and a static
 * output-device table. Playback is pushed through an optional output hook
 * so the library stays independent of any particular audio driver.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWAUDIO64_H
#define OWAUDIO64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWAUDIO_MAX_CHANNELS 16u  /* mixer channel slots           */
#define OWAUDIO_MAX_DEVICES   4u  /* output device slots           */
#define OWAUDIO_NAME_MAX     32u  /* stored device name length     */
#define OWAUDIO_VOLUME_MAX  100u  /* volume scale is 0..100        */

/* Pass as the channel to address the master volume / mute. */
#define OWAUDIO_MASTER_CHANNEL 0xFFFFFFFFu

typedef struct {
    uint32_t id;
    uint32_t volume;         /* 0..100                            */
    uint32_t sample_rate;
    uint64_t bytes_played;
    bool     used;
    bool     playing;
    bool     muted;
} owaudio_channel_t;

typedef struct {
    uint32_t id;
    char     name[OWAUDIO_NAME_MAX];
    uint32_t volume;         /* 0..100                            */
    bool     muted;
    bool     used;
} owaudio_device_t;

/* Output hook: submit `bytes` of mixed audio for `channel`. Returns false
 * when the device refused the buffer. */
typedef bool (*owaudio_output_fn)(void *ctx, uint32_t channel,
                                  const void *samples, size_t bytes,
                                  uint32_t sample_rate);

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize the mixer, register a default device and install the output
 * hook (which may be NULL for a software-only mixer). */
bool owaudio_init(owaudio_output_fn hook, void *ctx);

/* Submit a buffer for playback on a channel. Returns the channel index
 * (>= 0) on success, or -1 when the channel or buffer is invalid or the
 * hook rejected the data. */
int32_t owaudio_play(uint32_t channel, const void *samples, size_t bytes,
                     uint32_t sample_rate);

/* Stop a channel. */
bool owaudio_stop(uint32_t channel);

/* Set a per-channel volume, or the master volume when `channel` is
 * OWAUDIO_MASTER_CHANNEL. Values are clamped to 0..100. */
bool owaudio_set_volume(uint32_t channel, uint32_t volume);

/* Mute or unmute a channel / master. */
bool owaudio_set_mute(uint32_t channel, bool muted);

/* Copy up to `cap` registered output devices into `out`; returns the
 * number copied. */
uint32_t owaudio_get_devices(owaudio_device_t *out, uint32_t cap);

/* Copy up to `cap` active mixer channels into `out`. */
uint32_t owaudio_get_channels(owaudio_channel_t *out, uint32_t cap);

/* Register an additional output device. */
bool owaudio_register_device(const char *name, uint32_t *out_id);

/* Current master volume (0..100). */
uint32_t owaudio_get_master_volume(void);

/* True when the master output is muted. */
bool owaudio_is_master_muted(void);

#endif /* OWAUDIO64_H */
