/*
 * owlog64.c - OpenWindows Kernel Logging Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owlog64.h"

/* ── Internal state ───────────────────────────────────────────── */

static owlog_entry_t  owlog64_lines[OWLOG_MAX_LINES];
static uint32_t       owlog64_count = 0u;
static uint64_t       owlog64_sequence = 0u;
static owlog_level_t  owlog64_threshold = OWLOG_INFO;
static owlog_clock_fn owlog64_clock = NULL;
static void          *owlog64_clock_ctx = NULL;
static bool           owlog64_initialized = false;

/* ── Helpers ──────────────────────────────────────────────────── */

static void ow_memcpy(void *dst, const void *src, size_t len)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;

    for (i = 0u; i < len; ++i) {
        d[i] = s[i];
    }
}

static void ow_memmove_bytes(void *dst, const void *src, size_t len)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;

    if (d < s) {
        for (i = 0u; i < len; ++i) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (i = len; i > 0u; --i) {
            d[i - 1u] = s[i - 1u];
        }
    } else {
        /* Overlapping exact same range: nothing to do. */
    }
}

static void ow_memset(void *dst, uint8_t value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    size_t i;

    for (i = 0u; i < len; ++i) {
        d[i] = value;
    }
}

/* ── Implementation ───────────────────────────────────────────── */

bool owlog_init(owlog_level_t threshold)
{
    ow_memset(owlog64_lines, 0, sizeof(owlog64_lines));
    owlog64_count = 0u;
    owlog64_sequence = 0u;
    owlog64_threshold = threshold;
    owlog64_clock = NULL;
    owlog64_clock_ctx = NULL;
    owlog64_initialized = true;
    return true;
}

bool owlog_write(owlog_level_t level, const char *message, size_t length)
{
    owlog_entry_t *entry;
    size_t copy_len;

    if (!owlog64_initialized || message == NULL) {
        return false;
    }
    if ((uint32_t)level > (uint32_t)OWLOG_FATAL) {
        return false;
    }
    if ((uint32_t)level < (uint32_t)owlog64_threshold) {
        return false;               /* dropped by the threshold           */
    }

    copy_len = length;
    if (copy_len >= (size_t)OWLOG_LINE_MAX) {
        copy_len = (size_t)OWLOG_LINE_MAX - 1u;
    }

    if (owlog64_count < OWLOG_MAX_LINES) {
        entry = &owlog64_lines[owlog64_count];
        owlog64_count++;
    } else {
        /* Buffer full: drop the oldest line to preserve chronology. */
        ow_memmove_bytes(&owlog64_lines[0], &owlog64_lines[1],
                         (OWLOG_MAX_LINES - 1u) * sizeof(owlog_entry_t));
        entry = &owlog64_lines[OWLOG_MAX_LINES - 1u];
    }

    entry->sequence = owlog64_sequence++;
    entry->timestamp = (owlog64_clock != NULL)
                           ? owlog64_clock(owlog64_clock_ctx)
                           : 0u;
    entry->level = level;
    entry->length = (uint32_t)copy_len;
    ow_memcpy(entry->text, message, copy_len);
    entry->text[copy_len] = '\0';
    return true;
}

uint32_t owlog_flush(owlog_sink_fn sink, void *context)
{
    uint32_t i;

    if (!owlog64_initialized) {
        return 0u;
    }

    if (sink != NULL) {
        for (i = 0u; i < owlog64_count; ++i) {
            const owlog_entry_t *entry = &owlog64_lines[i];
            sink(entry->level, entry->text, (size_t)entry->length, context);
        }
    }

    i = owlog64_count;
    owlog_clear();
    return i;
}

owlog_level_t owlog_set_level(owlog_level_t threshold)
{
    owlog_level_t previous = owlog64_threshold;

    if ((uint32_t)threshold <= (uint32_t)OWLOG_FATAL) {
        owlog64_threshold = threshold;
    }
    return previous;
}

owlog_level_t owlog_get_level(void)
{
    return owlog64_threshold;
}

const owlog_entry_t *owlog_get_buffer(uint32_t *out_count)
{
    if (out_count != NULL) {
        *out_count = owlog64_count;
    }
    return owlog64_lines;
}

void owlog_clear(void)
{
    ow_memset(owlog64_lines, 0, sizeof(owlog64_lines));
    owlog64_count = 0u;
}

void owlog_set_clock(owlog_clock_fn clock, void *context)
{
    owlog64_clock = clock;
    owlog64_clock_ctx = context;
}

uint64_t owlog_count(void)
{
    return owlog64_sequence;
}

const char *owlog_level_name(owlog_level_t level)
{
    switch (level) {
    case OWLOG_TRACE: return "TRACE";
    case OWLOG_DEBUG: return "DEBUG";
    case OWLOG_INFO:  return "INFO";
    case OWLOG_WARN:  return "WARN";
    case OWLOG_ERROR: return "ERROR";
    case OWLOG_FATAL: return "FATAL";
    default:          return "UNKNOWN";
    }
}
