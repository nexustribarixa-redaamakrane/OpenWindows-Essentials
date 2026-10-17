/*
 * owevent64.h - OpenWindows Event Bus Library (.owd)
 *
 * A bounded publish/subscribe bus. Subscribers register a topic (or the
 * wildcard "*") together with a callback and an opaque context. Publishing a
 * topic dispatches synchronously to every matching subscriber; when a callback
 * itself publishes, the nested event is deferred into a fixed static queue and
 * drained by owevent_poll, so dispatch never recurses without bound.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWEVENT64_H
#define OWEVENT64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Capacity constants ───────────────────────────────────────── */

#define OWEVENT_MAX_SUBSCRIBERS 32u    /* Subscriber table size      */
#define OWEVENT_MAX_PENDING     16u    /* Deferred event queue depth  */
#define OWEVENT_TOPIC_MAX       32u    /* Topic bytes including NUL   */
#define OWEVENT_MAX_PAYLOAD    256u    /* Maximum event payload bytes */

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t owevent_status_t;

#define OWEVENT_OK             0x00000000u /* Success                 */
#define OWEVENT_ERR_PARAM      0x00000001u /* Invalid argument        */
#define OWEVENT_ERR_NOT_READY  0x00000002u /* Bus not initialized     */
#define OWEVENT_ERR_NO_MEM     0x00000003u /* Subscriber table full   */
#define OWEVENT_ERR_NOT_FOUND  0x00000004u /* Unknown subscription    */
#define OWEVENT_ERR_TOPIC      0x00000005u /* Topic empty or too long */
#define OWEVENT_ERR_FULL       0x00000006u /* Deferred queue full     */

/* ── Callback ─────────────────────────────────────────────────── */

/* Invoked for each matching subscriber. data is only valid for the
 * duration of the call; len may be zero. */
typedef void (*owevent_callback_fn)(const char *topic,
                                    const void *data,
                                    uint32_t len,
                                    void *ctx);

/* ── Statistics ───────────────────────────────────────────────── */

typedef struct {
    uint64_t published;   /* Events accepted by owevent_publish     */
    uint64_t dispatched;  /* Callback invocations delivered         */
    uint64_t dropped;     /* Deferred events discarded when full    */
    uint32_t subscribers; /* Active subscriptions                   */
    uint32_t pending;     /* Events waiting in the deferred queue   */
} owevent_stats_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Initialize (or reset) the event bus. */
owevent_status_t owevent_init(void);

/* Register a subscription. Topic must be non-empty and shorter than
 * OWEVENT_TOPIC_MAX; the single topic "*" receives every event. On
 * success the slot index is written to *out_handle. */
owevent_status_t owevent_subscribe(const char *topic,
                                   owevent_callback_fn cb,
                                   void *ctx,
                                   uint32_t *out_handle);

/* Remove a subscription previously returned by owevent_subscribe. */
owevent_status_t owevent_unsubscribe(uint32_t handle);

/* Publish an event. Dispatches immediately to matching subscribers, or
 * defers the event when called from inside a callback. The number of
 * callbacks invoked is written to *out_delivered when non-NULL. */
owevent_status_t owevent_publish(const char *topic,
                                 const void *data,
                                 uint32_t len,
                                 uint32_t *out_delivered);

/* Drain and dispatch events deferred during nested publishes. The total
 * number of callbacks invoked is written to *out_dispatched. */
owevent_status_t owevent_poll(uint32_t *out_dispatched);

/* Report the number of events waiting in the deferred queue. */
owevent_status_t owevent_pending_count(uint32_t *out_count);

/* Copy the bus counters. */
owevent_status_t owevent_get_stats(owevent_stats_t *out_stats);

#endif /* OWEVENT64_H */
