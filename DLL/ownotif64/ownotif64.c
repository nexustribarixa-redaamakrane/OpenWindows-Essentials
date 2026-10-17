/*
 * ownotif64.c - OpenWindows Notification Service Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ownotif64.h"

/* ── Internal state ───────────────────────────────────────────── */

typedef struct {
    char              name[OWNOTIF_NAME_MAX];
    ownotif_handler_fn handler;
    void             *ctx;
    bool              used;
} ownotif_channel_t;

static bool ownotif64_ready = false;
static ownotif_t ownotif64_queue[OWNOTIF_MAX_QUEUE];
static ownotif_channel_t ownotif64_channels[OWNOTIF_MAX_CHANNELS];
static uint32_t ownotif64_next_id = 1u;
static uint32_t ownotif64_count = 0u;

/* ── Internal helpers ─────────────────────────────────────────── */

static void ow_notif_copy_string(char *dst, const char *src)
{
    uint32_t i = 0u;

    if (src) {
        for (i = 0u; i + 1u < OWNOTIF_TEXT_MAX && src[i] != '\0'; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

static ownotif_t *ow_notif_find(uint32_t id)
{
    for (uint32_t i = 0u; i < OWNOTIF_MAX_QUEUE; ++i) {
        if (ownotif64_queue[i].active && ownotif64_queue[i].id == id) {
            return &ownotif64_queue[i];
        }
    }
    return NULL;
}

/* Dispatch a notification to one channel handler. */
static void ow_notif_dispatch_channel(uint32_t channel,
                                      const ownotif_t *n)
{
    if (channel >= OWNOTIF_MAX_CHANNELS) return;
    if (!ownotif64_channels[channel].used) return;
    if (!ownotif64_channels[channel].handler) return;

    ownotif64_channels[channel].handler(ownotif64_channels[channel].ctx, n);
}

/* ── Implementation ──────────────────────────────────────────── */

bool ownotif_init(void)
{
    for (uint32_t i = 0u; i < OWNOTIF_MAX_QUEUE; ++i) {
        ownotif64_queue[i].active = false;
        ownotif64_queue[i].id = 0u;
        ownotif64_queue[i].title[0] = '\0';
        ownotif64_queue[i].body[0] = '\0';
    }
    for (uint32_t i = 0u; i < OWNOTIF_MAX_CHANNELS; ++i) {
        ownotif64_channels[i].used = false;
        ownotif64_channels[i].handler = NULL;
        ownotif64_channels[i].ctx = NULL;
        ownotif64_channels[i].name[0] = '\0';
    }

    ownotif64_next_id = 1u;
    ownotif64_count = 0u;
    ownotif64_ready = true;
    return ownotif64_ready;
}

uint32_t ownotif_send(uint32_t channel, ownotif_level_t level,
                      const char *title, const char *body,
                      uint32_t ttl_ticks)
{
    if (!ownotif64_ready) return 0u;
    if (channel != OWNOTIF_CHANNEL_BROADCAST &&
        channel >= OWNOTIF_MAX_CHANNELS) {
        return 0u;
    }

    /* Find a free queue slot. */
    ownotif_t *slot = NULL;
    for (uint32_t i = 0u; i < OWNOTIF_MAX_QUEUE; ++i) {
        if (!ownotif64_queue[i].active) {
            slot = &ownotif64_queue[i];
            break;
        }
    }
    if (!slot) return 0u;

    slot->id = ownotif64_next_id++;
    if (ownotif64_next_id == 0u) ownotif64_next_id = 1u;
    slot->channel = channel;
    slot->level = level;
    slot->ttl_ticks = ttl_ticks;
    slot->active = true;
    ow_notif_copy_string(slot->title, title);
    ow_notif_copy_string(slot->body, body);
    ++ownotif64_count;

    /* Synchronous dispatch. The stored copy is passed so handlers can
     * only observe, not mutate, queue state. */
    if (channel == OWNOTIF_CHANNEL_BROADCAST) {
        for (uint32_t c = 0u; c < OWNOTIF_MAX_CHANNELS; ++c) {
            ow_notif_dispatch_channel(c, slot);
        }
    } else {
        ow_notif_dispatch_channel(channel, slot);
    }

    return slot->id;
}

bool ownotif_dismiss(uint32_t id)
{
    ownotif_t *n = ow_notif_find(id);
    if (!n) return false;

    n->active = false;
    if (ownotif64_count > 0u) --ownotif64_count;
    return true;
}

bool ownotif_register(const char *name, ownotif_handler_fn handler,
                      void *ctx, uint32_t *out_channel)
{
    if (!ownotif64_ready || !handler) return false;

    for (uint32_t i = 0u; i < OWNOTIF_MAX_CHANNELS; ++i) {
        if (ownotif64_channels[i].used) continue;

        ow_notif_copy_string(ownotif64_channels[i].name, name);
        ownotif64_channels[i].handler = handler;
        ownotif64_channels[i].ctx = ctx;
        ownotif64_channels[i].used = true;

        if (out_channel) *out_channel = i;
        return true;
    }

    return false;
}

uint32_t ownotif_get_pending(ownotif_t *out, uint32_t cap)
{
    if (!out || cap == 0u) return 0u;

    uint32_t copied = 0u;
    for (uint32_t i = 0u; i < OWNOTIF_MAX_QUEUE && copied < cap; ++i) {
        if (!ownotif64_queue[i].active) continue;
        out[copied] = ownotif64_queue[i];
        ++copied;
    }
    return copied;
}

uint32_t ownotif_get_count(void)
{
    return ownotif64_count;
}

bool ownotif_get(uint32_t id, ownotif_t *out)
{
    ownotif_t *n = ow_notif_find(id);
    if (!n || !out) return false;

    *out = *n;
    return true;
}

bool ownotif_clear(void)
{
    if (!ownotif64_ready) return false;

    for (uint32_t i = 0u; i < OWNOTIF_MAX_QUEUE; ++i) {
        ownotif64_queue[i].active = false;
    }
    ownotif64_count = 0u;
    return true;
}
