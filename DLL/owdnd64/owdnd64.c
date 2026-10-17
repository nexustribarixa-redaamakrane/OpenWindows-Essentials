/*
 * owdnd64.c - OpenWindows Drag and Drop Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdnd64.h"

/* ── Internal state ───────────────────────────────────────────── */

typedef struct {
    uint32_t          id;
    char              name[OWDND_NAME_MAX];
    owdnd_format_t    format;
    owdnd_deliver_fn  deliver;
    void             *ctx;
    bool              used;
} owdnd_target_t;

static bool owdnd64_ready = false;
static owdnd_target_t owdnd64_targets[OWDND_MAX_TARGETS];
static uint32_t owdnd64_next_id = 1u;
static uint32_t owdnd64_target_count = 0u;

static bool owdnd64_dragging = false;
static owdnd_format_t owdnd64_drag_format = OWDND_FORMAT_ANY;
static size_t owdnd64_payload_len = 0u;
static uint8_t owdnd64_payload[OWDND_MAX_PAYLOAD];

/* ── Internal helpers ─────────────────────────────────────────── */

static void owdnd_copy_bytes(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0u; i < len; ++i) d[i] = s[i];
}

static void owdnd_copy_string(char *dst, const char *src)
{
    uint32_t i = 0u;

    if (src) {
        for (i = 0u; i + 1u < OWDND_NAME_MAX && src[i] != '\0'; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

static owdnd_target_t *owdnd_find_target(uint32_t target_id)
{
    for (uint32_t i = 0u; i < OWDND_MAX_TARGETS; ++i) {
        if (owdnd64_targets[i].used && owdnd64_targets[i].id == target_id) {
            return &owdnd64_targets[i];
        }
    }
    return NULL;
}

static bool owdnd_drag_format_valid(owdnd_format_t format)
{
    switch (format) {
    case OWDND_FORMAT_TEXT:
    case OWDND_FORMAT_BITMAP:
    case OWDND_FORMAT_FILE_LIST:
    case OWDND_FORMAT_CUSTOM:
        return true;
    default:
        return false;
    }
}

static void owdnd_end_drag(void)
{
    owdnd64_dragging = false;
    owdnd64_drag_format = OWDND_FORMAT_ANY;
    owdnd64_payload_len = 0u;
}

/* ── Implementation ──────────────────────────────────────────── */

bool owdnd_init(void)
{
    for (uint32_t i = 0u; i < OWDND_MAX_TARGETS; ++i) {
        owdnd64_targets[i].used = false;
        owdnd64_targets[i].id = 0u;
        owdnd64_targets[i].deliver = NULL;
        owdnd64_targets[i].ctx = NULL;
        owdnd64_targets[i].name[0] = '\0';
    }

    owdnd64_next_id = 1u;
    owdnd64_target_count = 0u;
    owdnd_end_drag();
    owdnd64_ready = true;
    return owdnd64_ready;
}

bool owdnd_register_target(const char *name, owdnd_format_t format,
                           owdnd_deliver_fn deliver, void *ctx,
                           uint32_t *out_id)
{
    if (!owdnd64_ready) return false;
    if (!deliver) return false;
    if (format > OWDND_FORMAT_CUSTOM) return false;

    for (uint32_t i = 0u; i < OWDND_MAX_TARGETS; ++i) {
        if (owdnd64_targets[i].used) continue;

        owdnd_target_t *t = &owdnd64_targets[i];
        t->id = owdnd64_next_id++;
        if (owdnd64_next_id == 0u) owdnd64_next_id = 1u;
        t->format = format;
        t->deliver = deliver;
        t->ctx = ctx;
        t->used = true;
        owdnd_copy_string(t->name, name);
        ++owdnd64_target_count;

        if (out_id) *out_id = t->id;
        return true;
    }

    return false;
}

bool owdnd_begin_drag(const void *payload, size_t len, owdnd_format_t format)
{
    if (!owdnd64_ready) return false;
    if (!owdnd_drag_format_valid(format)) return false;
    if (len > (size_t)OWDND_MAX_PAYLOAD) return false;
    if (len != 0u && !payload) return false;

    if (len != 0u) {
        owdnd_copy_bytes(owdnd64_payload, payload, len);
    }

    owdnd64_payload_len = len;
    owdnd64_drag_format = format;
    owdnd64_dragging = true;
    return true;
}

bool owdnd_drop(uint32_t target_id)
{
    if (!owdnd64_ready || !owdnd64_dragging) return false;

    owdnd_target_t *target = owdnd_find_target(target_id);
    if (!target) return false;

    /* Format negotiation: ANY accepts everything. */
    if (target->format != OWDND_FORMAT_ANY &&
        target->format != owdnd64_drag_format) {
        return false;
    }

    bool accepted = target->deliver(target->ctx, owdnd64_payload,
                                    owdnd64_payload_len,
                                    owdnd64_drag_format);
    if (accepted) owdnd_end_drag();
    return accepted;
}

size_t owdnd_get_data(void *out, size_t cap, owdnd_format_t *out_format)
{
    if (out_format) {
        *out_format = owdnd64_dragging ? owdnd64_drag_format
                                       : OWDND_FORMAT_ANY;
    }
    if (!owdnd64_dragging || !out || cap == 0u) return 0u;

    size_t n = (owdnd64_payload_len < cap) ? owdnd64_payload_len : cap;
    owdnd_copy_bytes(out, owdnd64_payload, n);
    return n;
}

bool owdnd_cancel(void)
{
    if (!owdnd64_dragging) return false;
    owdnd_end_drag();
    return true;
}

bool owdnd_is_dragging(void)
{
    return owdnd64_dragging;
}

owdnd_format_t owdnd_get_drag_format(void)
{
    return owdnd64_drag_format;
}

size_t owdnd_get_payload_size(void)
{
    return owdnd64_payload_len;
}

uint32_t owdnd_get_target_count(void)
{
    return owdnd64_target_count;
}
