/*
 * owdnd64.h - OpenWindows Drag and Drop Library (.owd)
 *
 * Models a single active drag operation with a fixed-size payload. Drop
 * targets register a format filter and a delivery callback; dropping the
 * active payload invokes the matching target's callback.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDND64_H
#define OWDND64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWDND_MAX_TARGETS  16u   /* registered drop targets       */
#define OWDND_MAX_PAYLOAD   4096u /* active drag payload capacity  */
#define OWDND_NAME_MAX      32u  /* stored target name length     */

typedef enum {
    OWDND_FORMAT_ANY = 0,        /* target accepts everything     */
    OWDND_FORMAT_TEXT,
    OWDND_FORMAT_BITMAP,
    OWDND_FORMAT_FILE_LIST,
    OWDND_FORMAT_CUSTOM
} owdnd_format_t;

/* Target callback; returns true when the payload was accepted. */
typedef bool (*owdnd_deliver_fn)(void *ctx, const void *data, size_t len,
                                 owdnd_format_t format);

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize (and clear) the target table. */
bool owdnd_init(void);

/* Register a drop target accepting `format` (OWDND_FORMAT_ANY matches
 * everything). Returns the assigned target id through `out_id`. */
bool owdnd_register_target(const char *name, owdnd_format_t format,
                           owdnd_deliver_fn deliver, void *ctx,
                           uint32_t *out_id);

/* Begin dragging `payload` (copied into a static buffer) tagged with a
 * concrete format. Replaces any previous drag. */
bool owdnd_begin_drag(const void *payload, size_t len, owdnd_format_t format);

/* Deliver the active payload to a registered target. On success the drag
 * ends; on failure the drag remains active so the caller may retry. */
bool owdnd_drop(uint32_t target_id);

/* Copy the active payload into `out` (at most `cap` bytes) and report its
 * format. Returns the number of bytes copied, 0 when idle. */
size_t owdnd_get_data(void *out, size_t cap, owdnd_format_t *out_format);

/* Abort the active drag. Returns true when a drag was in progress. */
bool owdnd_cancel(void);

/* True while a payload is being dragged. */
bool owdnd_is_dragging(void);

/* Format of the active payload (OWDND_FORMAT_ANY when idle). */
owdnd_format_t owdnd_get_drag_format(void);

/* Size in bytes of the active payload. */
size_t owdnd_get_payload_size(void);

/* Number of registered targets. */
uint32_t owdnd_get_target_count(void);

#endif /* OWDND64_H */
