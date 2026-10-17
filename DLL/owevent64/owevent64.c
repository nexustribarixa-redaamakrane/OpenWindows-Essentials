/*
 * owevent64.c - OpenWindows Event Bus Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owevent64.h"

/* ── Internal state ───────────────────────────────────────────── */

typedef struct {
    bool                 active;
    char                 topic[OWEVENT_TOPIC_MAX];
    owevent_callback_fn  cb;
    void                *ctx;
    uint64_t             deliveries;
} owevent_subscriber_t;

typedef struct {
    char     topic[OWEVENT_TOPIC_MAX];
    uint32_t len;
    uint8_t  data[OWEVENT_MAX_PAYLOAD];
} owevent_event_t;

static owevent_subscriber_t g_subs[OWEVENT_MAX_SUBSCRIBERS];
static owevent_event_t      g_pending[OWEVENT_MAX_PENDING];
static uint32_t             g_pending_head = 0u;
static uint32_t             g_pending_tail = 0u;
static uint32_t             g_pending_count = 0u;
static bool                 g_dispatching = false;
static bool                 g_ready = false;
static uint64_t             g_published = 0u;
static uint64_t             g_dispatched = 0u;
static uint64_t             g_dropped = 0u;

/* ── Internal helpers ─────────────────────────────────────────── */

static void owevent_memzero(void *dst, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < n; ++i) d[i] = 0u;
}

static void owevent_memcopy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < n; ++i) d[i] = s[i];
}

static uint32_t owevent_strlen(const char *s)
{
    uint32_t n = 0u;
    if (!s) return 0u;
    while (s[n] != '\0') n++;
    return n;
}

static void owevent_strcopy(char *dst, const char *src, size_t cap)
{
    size_t i = 0u;
    if (!dst || cap == 0u) return;
    if (src) {
        while (i + 1u < cap && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static bool owevent_streq(const char *a, const char *b)
{
    uint32_t i = 0u;
    if (!a || !b) return false;
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

/* "*" matches any topic; otherwise require an exact match. */
static bool owevent_topic_match(const char *pattern, const char *topic)
{
    if (!pattern || !topic) return false;
    if (pattern[0] == '*' && pattern[1] == '\0') return true;
    return owevent_streq(pattern, topic);
}

static owevent_status_t owevent_require_ready(void)
{
    return g_ready ? OWEVENT_OK : OWEVENT_ERR_NOT_READY;
}

/* Deliver one event to every matching subscriber. */
static uint32_t owevent_dispatch(const char *topic,
                                 const void *data,
                                 uint32_t len)
{
    uint32_t delivered = 0u;
    uint32_t i;
    for (i = 0u; i < OWEVENT_MAX_SUBSCRIBERS; ++i) {
        owevent_subscriber_t *s = &g_subs[i];
        if (!s->active || !s->cb) continue;
        if (!owevent_topic_match(s->topic, topic)) continue;
        s->deliveries++;
        s->cb(topic, data, len, s->ctx);
        delivered++;
    }
    return delivered;
}

static owevent_status_t owevent_enqueue(const char *topic,
                                        const void *data,
                                        uint32_t len)
{
    owevent_event_t *e;
    if (g_pending_count >= OWEVENT_MAX_PENDING) {
        g_dropped++;
        return OWEVENT_ERR_FULL;
    }
    e = &g_pending[g_pending_tail];
    owevent_strcopy(e->topic, topic, sizeof(e->topic));
    e->len = len;
    if (len != 0u) owevent_memcopy(e->data, data, len);
    g_pending_tail = (g_pending_tail + 1u) % OWEVENT_MAX_PENDING;
    g_pending_count++;
    return OWEVENT_OK;
}

static bool owevent_dequeue(owevent_event_t *out)
{
    if (g_pending_count == 0u) return false;
    *out = g_pending[g_pending_head];
    g_pending_head = (g_pending_head + 1u) % OWEVENT_MAX_PENDING;
    g_pending_count--;
    return true;
}

/* ── Implementation ───────────────────────────────────────────── */

owevent_status_t owevent_init(void)
{
    owevent_memzero(g_subs, sizeof(g_subs));
    owevent_memzero(g_pending, sizeof(g_pending));
    g_pending_head = 0u;
    g_pending_tail = 0u;
    g_pending_count = 0u;
    g_dispatching = false;
    g_published = 0u;
    g_dispatched = 0u;
    g_dropped = 0u;
    g_ready = true;
    return OWEVENT_OK;
}

owevent_status_t owevent_subscribe(const char *topic,
                                   owevent_callback_fn cb,
                                   void *ctx,
                                   uint32_t *out_handle)
{
    owevent_status_t st = owevent_require_ready();
    uint32_t i;

    if (st != OWEVENT_OK) return st;
    if (!topic || !cb || !out_handle) return OWEVENT_ERR_PARAM;
    if (topic[0] == '\0' || owevent_strlen(topic) + 1u > OWEVENT_TOPIC_MAX) {
        return OWEVENT_ERR_TOPIC;
    }

    for (i = 0u; i < OWEVENT_MAX_SUBSCRIBERS; ++i) {
        owevent_subscriber_t *s = &g_subs[i];
        if (!s->active) {
            s->active = true;
            owevent_strcopy(s->topic, topic, sizeof(s->topic));
            s->cb = cb;
            s->ctx = ctx;
            s->deliveries = 0u;
            *out_handle = i;
            return OWEVENT_OK;
        }
    }
    return OWEVENT_ERR_NO_MEM;
}

owevent_status_t owevent_unsubscribe(uint32_t handle)
{
    owevent_status_t st = owevent_require_ready();
    owevent_subscriber_t *s;

    if (st != OWEVENT_OK) return st;
    if (handle >= OWEVENT_MAX_SUBSCRIBERS) return OWEVENT_ERR_NOT_FOUND;

    s = &g_subs[handle];
    if (!s->active) return OWEVENT_ERR_NOT_FOUND;

    owevent_memzero(s, sizeof(*s));
    return OWEVENT_OK;
}

owevent_status_t owevent_publish(const char *topic,
                                 const void *data,
                                 uint32_t len,
                                 uint32_t *out_delivered)
{
    owevent_status_t st = owevent_require_ready();
    uint32_t delivered;

    if (st != OWEVENT_OK) return st;
    if (out_delivered) *out_delivered = 0u;
    if (!topic || topic[0] == '\0') return OWEVENT_ERR_PARAM;
    if (owevent_strlen(topic) + 1u > OWEVENT_TOPIC_MAX) {
        return OWEVENT_ERR_TOPIC;
    }
    if (len > OWEVENT_MAX_PAYLOAD) return OWEVENT_ERR_PARAM;
    if (len != 0u && !data) return OWEVENT_ERR_PARAM;

    g_published++;

    /* A publish from inside a callback is deferred to owevent_poll so the
     * dispatch stack cannot grow without bound. */
    if (g_dispatching) {
        return owevent_enqueue(topic, data, len);
    }

    g_dispatching = true;
    delivered = owevent_dispatch(topic, data, len);
    g_dispatching = false;

    g_dispatched += delivered;
    if (out_delivered) *out_delivered = delivered;
    return OWEVENT_OK;
}

owevent_status_t owevent_poll(uint32_t *out_dispatched)
{
    owevent_status_t st = owevent_require_ready();
    uint32_t total = 0u;
    uint32_t budget = OWEVENT_MAX_PENDING * 4u;
    owevent_event_t ev;

    if (st != OWEVENT_OK) return st;

    g_dispatching = true;
    while (budget > 0u) {
        budget--;
        if (!owevent_dequeue(&ev)) break;
        total += owevent_dispatch(ev.topic, ev.data, ev.len);
    }
    g_dispatching = false;

    g_dispatched += total;
    if (out_dispatched) *out_dispatched = total;
    return OWEVENT_OK;
}

owevent_status_t owevent_pending_count(uint32_t *out_count)
{
    owevent_status_t st = owevent_require_ready();
    if (st != OWEVENT_OK) return st;
    if (!out_count) return OWEVENT_ERR_PARAM;
    *out_count = g_pending_count;
    return OWEVENT_OK;
}

owevent_status_t owevent_get_stats(owevent_stats_t *out_stats)
{
    owevent_status_t st = owevent_require_ready();
    uint32_t i;

    if (st != OWEVENT_OK) return st;
    if (!out_stats) return OWEVENT_ERR_PARAM;

    out_stats->published = g_published;
    out_stats->dispatched = g_dispatched;
    out_stats->dropped = g_dropped;
    out_stats->subscribers = 0u;
    for (i = 0u; i < OWEVENT_MAX_SUBSCRIBERS; ++i) {
        if (g_subs[i].active) out_stats->subscribers++;
    }
    out_stats->pending = g_pending_count;
    return OWEVENT_OK;
}
