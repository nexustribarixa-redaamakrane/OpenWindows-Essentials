/*
 * owfifo64.c - OpenWindows FIFO Ring Buffer Library (.owd)
 *
 * Single-producer/single-consumer byte queue. All indices are reduced
 * modulo the capacity on every mutation.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owfifo64.h"

/* ── Implementation ───────────────────────────────────────────── */

bool owfifo_init(owfifo_t *fifo, uint8_t *buf, uint32_t capacity)
{
    if (fifo == NULL || buf == NULL || capacity == 0u) return false;

    fifo->buf = buf;
    fifo->capacity = capacity;
    fifo->head = 0u;
    fifo->tail = 0u;
    fifo->used = 0u;
    fifo->initialized = true;
    return true;
}

bool owfifo_push(owfifo_t *fifo, uint8_t value)
{
    if (fifo == NULL || !fifo->initialized) return false;
    if (fifo->used >= fifo->capacity) return false;

    fifo->buf[fifo->tail] = value;
    fifo->tail = (fifo->tail + 1u) % fifo->capacity;
    fifo->used++;
    return true;
}

bool owfifo_pop(owfifo_t *fifo, uint8_t *out)
{
    if (fifo == NULL || !fifo->initialized || out == NULL) return false;
    if (fifo->used == 0u) return false;

    *out = fifo->buf[fifo->head];
    fifo->head = (fifo->head + 1u) % fifo->capacity;
    fifo->used--;
    return true;
}

bool owfifo_peek(const owfifo_t *fifo, uint8_t *out)
{
    if (fifo == NULL || !fifo->initialized || out == NULL) return false;
    if (fifo->used == 0u) return false;

    *out = fifo->buf[fifo->head];
    return true;
}

uint32_t owfifo_count(const owfifo_t *fifo)
{
    if (fifo == NULL || !fifo->initialized) return 0u;
    return fifo->used;
}

bool owfifo_is_empty(const owfifo_t *fifo)
{
    if (fifo == NULL || !fifo->initialized) return true;
    return fifo->used == 0u;
}

bool owfifo_is_full(const owfifo_t *fifo)
{
    if (fifo == NULL || !fifo->initialized) return false;
    return fifo->used >= fifo->capacity;
}
