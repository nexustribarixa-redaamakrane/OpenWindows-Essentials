/*
 * owmsg64.c - OpenWindows Message Queue Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owmsg64.h"

/* ── Internal state ───────────────────────────────────────────── */

typedef struct {
    uint32_t       head;   /* Index of oldest queued message        */
    uint32_t       tail;   /* Index of next free slot               */
    uint32_t       count;
    owmsg_stats_t  stats;
    owmsg_message_t slots[OWMSG_QUEUE_DEPTH];
} owmsg_queue_t;

static owmsg_queue_t g_queues[OWMSG_MAX_QUEUES];
static bool          g_ready = false;

/* ── Internal helpers ─────────────────────────────────────────── */

static void owmsg_memzero(void *dst, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < n; ++i) d[i] = 0u;
}

static void owmsg_memcopy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < n; ++i) d[i] = s[i];
}

static owmsg_queue_t *owmsg_queue(uint32_t id)
{
    if (id >= OWMSG_MAX_QUEUES) return NULL;
    return &g_queues[id];
}

static owmsg_status_t owmsg_require_ready(void)
{
    return g_ready ? OWMSG_OK : OWMSG_ERR_NOT_READY;
}

/* ── Implementation ───────────────────────────────────────────── */

owmsg_status_t owmsg_init(void)
{
    uint32_t i;
    for (i = 0u; i < OWMSG_MAX_QUEUES; ++i) {
        owmsg_memzero(&g_queues[i], sizeof(g_queues[i]));
    }
    g_ready = true;
    return OWMSG_OK;
}

owmsg_status_t owmsg_send(uint32_t queue,
                          uint32_t type,
                          const void *data,
                          uint32_t len)
{
    owmsg_queue_t  *q;
    owmsg_message_t *slot;
    owmsg_status_t  st = owmsg_require_ready();

    if (st != OWMSG_OK) return st;
    if (len > OWMSG_PAYLOAD_SIZE) return OWMSG_ERR_TOO_LARGE;
    if (len != 0u && !data) return OWMSG_ERR_PARAM;

    q = owmsg_queue(queue);
    if (!q) return OWMSG_ERR_BAD_QUEUE;
    if (q->count >= OWMSG_QUEUE_DEPTH) {
        q->stats.dropped++;
        return OWMSG_ERR_FULL;
    }

    slot = &q->slots[q->tail];
    slot->type = type;
    slot->len = len;
    if (len != 0u) {
        owmsg_memcopy(slot->data, data, len);
    }

    q->tail = (q->tail + 1u) % OWMSG_QUEUE_DEPTH;
    q->count++;
    q->stats.sent++;
    return OWMSG_OK;
}

owmsg_status_t owmsg_recv(uint32_t queue, owmsg_message_t *out)
{
    owmsg_queue_t  *q;
    owmsg_status_t  st = owmsg_require_ready();

    if (st != OWMSG_OK) return st;
    if (!out) return OWMSG_ERR_PARAM;

    q = owmsg_queue(queue);
    if (!q) return OWMSG_ERR_BAD_QUEUE;
    if (q->count == 0u) return OWMSG_ERR_EMPTY;

    *out = q->slots[q->head];
    q->head = (q->head + 1u) % OWMSG_QUEUE_DEPTH;
    q->count--;
    q->stats.received++;
    return OWMSG_OK;
}

owmsg_status_t owmsg_peek(uint32_t queue, owmsg_message_t *out)
{
    owmsg_queue_t  *q;
    owmsg_status_t  st = owmsg_require_ready();

    if (st != OWMSG_OK) return st;
    if (!out) return OWMSG_ERR_PARAM;

    q = owmsg_queue(queue);
    if (!q) return OWMSG_ERR_BAD_QUEUE;
    if (q->count == 0u) return OWMSG_ERR_EMPTY;

    *out = q->slots[q->head];
    return OWMSG_OK;
}

owmsg_status_t owmsg_flush(uint32_t queue, uint32_t *out_discarded)
{
    owmsg_queue_t  *q;
    owmsg_status_t  st = owmsg_require_ready();
    uint32_t        n;

    if (st != OWMSG_OK) return st;

    q = owmsg_queue(queue);
    if (!q) return OWMSG_ERR_BAD_QUEUE;

    n = q->count;
    while (q->count != 0u) {
        owmsg_memzero(&q->slots[q->head], sizeof(q->slots[q->head]));
        q->head = (q->head + 1u) % OWMSG_QUEUE_DEPTH;
        q->count--;
    }
    q->head = 0u;
    q->tail = 0u;
    q->stats.discarded += n;

    if (out_discarded) *out_discarded = n;
    return OWMSG_OK;
}

owmsg_status_t owmsg_count(uint32_t queue, uint32_t *out_count)
{
    owmsg_queue_t  *q;
    owmsg_status_t  st = owmsg_require_ready();

    if (st != OWMSG_OK) return st;
    if (!out_count) return OWMSG_ERR_PARAM;

    q = owmsg_queue(queue);
    if (!q) return OWMSG_ERR_BAD_QUEUE;

    *out_count = q->count;
    return OWMSG_OK;
}

owmsg_status_t owmsg_get_stats(uint32_t queue, owmsg_stats_t *out_stats)
{
    owmsg_queue_t  *q;
    owmsg_status_t  st = owmsg_require_ready();

    if (st != OWMSG_OK) return st;
    if (!out_stats) return OWMSG_ERR_PARAM;

    q = owmsg_queue(queue);
    if (!q) return OWMSG_ERR_BAD_QUEUE;

    *out_stats = q->stats;
    return OWMSG_OK;
}
