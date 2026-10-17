/*
 * owmsg64.h - OpenWindows Message Queue Library (.owd)
 *
 * Fixed-capacity FIFO message queues for intra-process communication. Every
 * queue owns a ring of statically allocated message slots carrying a fixed
 * maximum payload; nothing is ever allocated on a heap. Queues are addressed
 * by a small integer id in the range [0, OWMSG_MAX_QUEUES).
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWMSG64_H
#define OWMSG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Capacity constants ───────────────────────────────────────── */

#define OWMSG_MAX_QUEUES     8u     /* Number of independent queues  */
#define OWMSG_QUEUE_DEPTH   16u     /* Slots per queue               */
#define OWMSG_PAYLOAD_SIZE 256u     /* Maximum bytes per message     */

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t owmsg_status_t;

#define OWMSG_OK            0x00000000u /* Success                   */
#define OWMSG_ERR_PARAM     0x00000001u /* Invalid argument          */
#define OWMSG_ERR_NOT_READY 0x00000002u /* Queue layer not initialized */
#define OWMSG_ERR_BAD_QUEUE 0x00000003u /* Queue id out of range     */
#define OWMSG_ERR_FULL      0x00000004u /* Queue has no free slot    */
#define OWMSG_ERR_EMPTY     0x00000005u /* Queue holds no message    */
#define OWMSG_ERR_TOO_LARGE 0x00000006u /* Payload exceeds the slot  */

/* ── Message ──────────────────────────────────────────────────── */

typedef struct {
    uint32_t type;                      /* Caller-defined class      */
    uint32_t len;                       /* Valid bytes in data       */
    uint8_t  data[OWMSG_PAYLOAD_SIZE];
} owmsg_message_t;

/* ── Per-queue statistics ─────────────────────────────────────── */

typedef struct {
    uint64_t sent;
    uint64_t received;
    uint64_t dropped;
    uint64_t discarded;
} owmsg_stats_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Reset every queue to the empty state. Must be called first. */
owmsg_status_t owmsg_init(void);

/* Enqueue a message of the given type and payload (FIFO tail). */
owmsg_status_t owmsg_send(uint32_t queue,
                          uint32_t type,
                          const void *data,
                          uint32_t len);

/* Dequeue the oldest message into *out (FIFO head). */
owmsg_status_t owmsg_recv(uint32_t queue, owmsg_message_t *out);

/* Read the oldest message without removing it. */
owmsg_status_t owmsg_peek(uint32_t queue, owmsg_message_t *out);

/* Discard every queued message. Returns the number discarded through
 * *out_discarded when non-NULL. */
owmsg_status_t owmsg_flush(uint32_t queue, uint32_t *out_discarded);

/* Report how many messages are currently queued. */
owmsg_status_t owmsg_count(uint32_t queue, uint32_t *out_count);

/* Copy the queue's running counters. */
owmsg_status_t owmsg_get_stats(uint32_t queue, owmsg_stats_t *out_stats);

#endif /* OWMSG64_H */
