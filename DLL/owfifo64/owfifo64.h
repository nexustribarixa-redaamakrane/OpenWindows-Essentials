/*
 * owfifo64.h - OpenWindows FIFO Ring Buffer Library (.owd)
 *
 * Byte-oriented ring buffer over a caller-provided buffer. A separate
 * occupancy counter distinguishes the full and empty states, so up to
 * capacity bytes can be queued.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWFIFO64_H
#define OWFIFO64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t *buf;          /* caller-provided storage      */
    uint32_t capacity;     /* bytes in buf                 */
    uint32_t head;         /* read index                   */
    uint32_t tail;         /* write index                  */
    uint32_t used;         /* queued byte count            */
    bool     initialized;
} owfifo_t;

/* Bind fifo to buf, which must hold capacity bytes. */
bool owfifo_init(owfifo_t *fifo, uint8_t *buf, uint32_t capacity);

/* Append one byte. Returns false when the ring is full. */
bool owfifo_push(owfifo_t *fifo, uint8_t value);

/* Remove the oldest byte into *out. Returns false when empty. */
bool owfifo_pop(owfifo_t *fifo, uint8_t *out);

/* Copy the oldest byte into *out without removing it. */
bool owfifo_peek(const owfifo_t *fifo, uint8_t *out);

/* Number of queued bytes. */
uint32_t owfifo_count(const owfifo_t *fifo);

/* True when nothing is queued. */
bool owfifo_is_empty(const owfifo_t *fifo);

/* True when no further bytes can be pushed. */
bool owfifo_is_full(const owfifo_t *fifo);

#endif /* OWFIFO64_H */
