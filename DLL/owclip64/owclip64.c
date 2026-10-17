/*
 * owclip64.c - OpenWindows Clipboard Manager (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owclip64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owclip64_ready = false;
static owclip_format_t owclip64_format = OWCLIP_FORMAT_NONE;
static size_t owclip64_size = 0u;
static uint8_t owclip64_buffer[OWCLIP_MAX_BYTES];

/* ── Internal helpers ─────────────────────────────────────────── */

static void owclip_copy_bytes(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0; i < len; ++i) d[i] = s[i];
}

/* True when `format` is one of the defined clipboard formats. */
static bool owclip_format_valid(owclip_format_t format)
{
    switch (format) {
    case OWCLIP_FORMAT_TEXT:
    case OWCLIP_FORMAT_UNICODE:
    case OWCLIP_FORMAT_BITMAP:
    case OWCLIP_FORMAT_FILE_LIST:
    case OWCLIP_FORMAT_CUSTOM:
        return true;
    default:
        return false;
    }
}

/* ── Implementation ──────────────────────────────────────────── */

bool owclip_init(void)
{
    owclip64_format = OWCLIP_FORMAT_NONE;
    owclip64_size = 0u;
    for (size_t i = 0; i < (size_t)OWCLIP_MAX_BYTES; ++i) {
        owclip64_buffer[i] = 0u;
    }
    owclip64_ready = true;
    return owclip64_ready;
}

bool owclip_copy(owclip_format_t format, const void *data, size_t len)
{
    if (!owclip64_ready) return false;
    if (!owclip_format_valid(format)) return false;
    if (len > (size_t)OWCLIP_MAX_BYTES) return false;
    if (len != 0u && !data) return false;

    if (len != 0u) {
        owclip_copy_bytes(owclip64_buffer, data, len);
    }

    owclip64_format = format;
    owclip64_size = len;
    return true;
}

size_t owclip_paste(void *out, size_t cap, owclip_format_t *out_format)
{
    if (!owclip64_ready) {
        if (out_format) *out_format = OWCLIP_FORMAT_NONE;
        return 0u;
    }

    if (out_format) *out_format = owclip64_format;

    if (!out || cap == 0u || owclip64_size == 0u) return 0u;

    size_t n = (owclip64_size < cap) ? owclip64_size : cap;
    owclip_copy_bytes(out, owclip64_buffer, n);
    return n;
}

bool owclip_clear(void)
{
    if (!owclip64_ready) return false;

    owclip64_format = OWCLIP_FORMAT_NONE;
    owclip64_size = 0u;
    return true;
}

owclip_format_t owclip_get_format(void)
{
    return owclip64_format;
}

size_t owclip_get_size(void)
{
    return owclip64_size;
}

bool owclip_is_empty(void)
{
    return owclip64_size == 0u;
}
