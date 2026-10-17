/*
 * owclip64.h - OpenWindows Clipboard Manager (.owd)
 *
 * Single-format clipboard backed by a fixed static buffer. Callers copy
 * raw bytes tagged with a format identifier, then paste them back into a
 * caller-provided buffer. No heap, no ownership transfer.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWCLIP64_H
#define OWCLIP64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWCLIP_MAX_BYTES 8192u   /* static clipboard capacity */

typedef enum {
    OWCLIP_FORMAT_NONE = 0,      /* empty / no format          */
    OWCLIP_FORMAT_TEXT,          /* UTF-8 / ASCII text         */
    OWCLIP_FORMAT_UNICODE,       /* UTF-16 code units          */
    OWCLIP_FORMAT_BITMAP,        /* packed pixel data          */
    OWCLIP_FORMAT_FILE_LIST,     /* newline separated paths    */
    OWCLIP_FORMAT_CUSTOM         /* caller-defined payload      */
} owclip_format_t;

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize (and clear) the clipboard. */
bool owclip_init(void);

/* Store `len` bytes tagged with `format`, replacing any previous content.
 * `len` of 0 is allowed; otherwise `data` must be non-NULL. */
bool owclip_copy(owclip_format_t format, const void *data, size_t len);

/* Copy the clipboard into `out` (at most `cap` bytes). Returns the number
 * of bytes written; writes the stored format through `out_format`. */
size_t owclip_paste(void *out, size_t cap, owclip_format_t *out_format);

/* Discard the current clipboard content. */
bool owclip_clear(void);

/* Format identifier of the current content (OWCLIP_FORMAT_NONE if empty). */
owclip_format_t owclip_get_format(void);

/* Number of bytes currently stored. */
size_t owclip_get_size(void);

/* True when there is no stored content. */
bool owclip_is_empty(void);

#endif /* OWCLIP64_H */
