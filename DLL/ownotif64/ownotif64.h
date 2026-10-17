/*
 * ownotif64.h - OpenWindows Notification Service Library (.owd)
 *
 * Static notification queue with per-channel handlers. Sending a
 * notification stores it in the queue and dispatches a copy to the
 * handler registered on its channel (or to every handler for the
 * broadcast channel).
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWNOTIF64_H
#define OWNOTIF64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWNOTIF_MAX_QUEUE     16u  /* pending notification slots     */
#define OWNOTIF_MAX_CHANNELS   8u  /* registrable channels           */
#define OWNOTIF_TEXT_MAX      96u  /* stored title/body length       */
#define OWNOTIF_NAME_MAX      32u  /* stored channel name length     */

/* Send to every registered channel. */
#define OWNOTIF_CHANNEL_BROADCAST 0xFFFFFFFFu

typedef enum {
    OWNOTIF_LEVEL_INFO = 0,
    OWNOTIF_LEVEL_SUCCESS,
    OWNOTIF_LEVEL_WARN,
    OWNOTIF_LEVEL_ERROR
} ownotif_level_t;

typedef struct {
    uint32_t         id;
    uint32_t         channel;
    ownotif_level_t  level;
    uint32_t         ttl_ticks;
    char             title[OWNOTIF_TEXT_MAX];
    char             body[OWNOTIF_TEXT_MAX];
    bool             active;
} ownotif_t;

/* Channel handler, invoked synchronously by ow_notif_send(). */
typedef void (*ownotif_handler_fn)(void *ctx, const ownotif_t *notification);

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize (and clear) the queue and channel table. */
bool ownotif_init(void);

/* Queue a notification and dispatch it. Returns the new notification id,
 * or 0 when the queue is full or the channel is invalid. */
uint32_t ownotif_send(uint32_t channel, ownotif_level_t level,
                      const char *title, const char *body,
                      uint32_t ttl_ticks);

/* Remove a pending notification. Returns false when unknown. */
bool ownotif_dismiss(uint32_t id);

/* Register a channel handler and return its channel index. */
bool ownotif_register(const char *name, ownotif_handler_fn handler,
                      void *ctx, uint32_t *out_channel);

/* Copy up to `cap` pending notifications into `out` in queue order.
 * Returns the number copied. */
uint32_t ownotif_get_pending(ownotif_t *out, uint32_t cap);

/* Number of currently pending notifications. */
uint32_t ownotif_get_count(void);

/* Fetch a single pending notification by id. */
bool ownotif_get(uint32_t id, ownotif_t *out);

/* Drop every pending notification. */
bool ownotif_clear(void);

#endif /* OWNOTIF64_H */
