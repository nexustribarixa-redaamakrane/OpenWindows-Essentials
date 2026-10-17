/*
 * owlog64.h - OpenWindows Kernel Logging Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Fixed-capacity ring of timestamped log lines with a configurable
 * severity threshold. Lines below the threshold are dropped. Buffered
 * lines can be drained to a pluggable sink.
 */

#ifndef OWLOG64_H
#define OWLOG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Configuration ────────────────────────────────────────────────── */

#define OWLOG_LEVEL_COUNT   6u      /* number of severity levels        */
#define OWLOG_LINE_MAX      128u    /* max characters per log line      */
#define OWLOG_MAX_LINES     64u     /* ring capacity                    */

/* ── Public types ─────────────────────────────────────────────────── */

typedef enum {
    OWLOG_TRACE = 0,                /* most verbose                     */
    OWLOG_DEBUG = 1,
    OWLOG_INFO  = 2,
    OWLOG_WARN  = 3,
    OWLOG_ERROR = 4,
    OWLOG_FATAL = 5                 /* always recorded                  */
} owlog_level_t;

typedef struct {
    uint64_t     sequence;          /* monotonic record number          */
    uint64_t     timestamp;         /* value from the clock hook        */
    owlog_level_t level;
    uint32_t     length;            /* characters in `text`             */
    char         text[OWLOG_LINE_MAX];
} owlog_entry_t;

/* Destination for owlog_flush(); may be NULL to just drop the backlog. */
typedef void (*owlog_sink_fn)(owlog_level_t level, const char *text,
                              size_t length, void *context);

/* Source of timestamps; may be NULL to record zero. */
typedef uint64_t (*owlog_clock_fn)(void *context);

/* ── API ──────────────────────────────────────────────────────────── */

/* Reset the buffer and set the minimum recorded severity. */
bool owlog_init(owlog_level_t threshold);

/* Append a line at `level`. Returns false if dropped by the threshold. */
bool owlog_write(owlog_level_t level, const char *message, size_t length);

/* Drain every buffered line to `sink` and clear the buffer. */
uint32_t owlog_flush(owlog_sink_fn sink, void *context);

/* Change the severity threshold; returns the previous threshold. */
owlog_level_t owlog_set_level(owlog_level_t threshold);

/* Current severity threshold. */
owlog_level_t owlog_get_level(void);

/* Chronological view of the buffer; `*out_count` receives the line count. */
const owlog_entry_t *owlog_get_buffer(uint32_t *out_count);

/* Drop all buffered lines without flushing. */
void owlog_clear(void);

/* Install a timestamp source used for subsequent records. */
void owlog_set_clock(owlog_clock_fn clock, void *context);

/* Total number of records ever accepted. */
uint64_t owlog_count(void);

/* Human-readable name for a severity level. */
const char *owlog_level_name(owlog_level_t level);

#endif /* OWLOG64_H */
