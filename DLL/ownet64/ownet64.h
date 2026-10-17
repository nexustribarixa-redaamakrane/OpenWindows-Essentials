/*
 * ownet64.h - OpenWindows Network Stack Library (.owd)
 *
 * Minimal IPv4 network stack that owns a static socket table and routes all
 * datagram traffic through a pluggable link-layer vtable. The stack performs
 * no allocation: sockets live in a fixed-size pool inside the stack context
 * and every transmit/receive buffer is caller-provided.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWNET64_H
#define OWNET64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Capacity and layout constants ────────────────────────────── */

#define OWNET_MAX_SOCKETS      32u   /* Socket table capacity         */
#define OWNET_MTU              1500u /* Maximum link-layer payload    */
#define OWNET_DEFAULT_TTL      64u   /* Default IPv4 time-to-live     */

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t ownet_status_t;

#define OWNET_OK               0x00000000u /* Success                   */
#define OWNET_ERR_PARAM        0x00000001u /* Invalid parameter         */
#define OWNET_ERR_NOT_READY    0x00000002u /* Stack not initialized     */
#define OWNET_ERR_NO_MEM       0x00000003u /* Socket table exhausted    */
#define OWNET_ERR_NOT_FOUND    0x00000004u /* Unknown socket handle     */
#define OWNET_ERR_STATE        0x00000005u /* Operation invalid in state*/
#define OWNET_ERR_LINK         0x00000006u /* Link-layer transmit error */
#define OWNET_ERR_TRUNCATED    0x00000007u /* Destination buffer too small */
#define OWNET_ERR_ADDR_IN_USE  0x00000008u /* Endpoint already bound    */
#define OWNET_ERR_WOULD_BLOCK  0x00000009u /* No data currently queued  */

/* ── Protocol / socket types ──────────────────────────────────── */

typedef enum {
    OWNET_PROTO_UDP = 0,   /* Connectionless datagram socket         */
    OWNET_PROTO_TCP = 1    /* Connection-oriented (handshake ATM)    */
} ownet_proto_t;

typedef enum {
    OWNET_SOCK_CLOSED = 0, /* Slot unused                            */
    OWNET_SOCK_OPENED,     /* Allocated, not yet bound/connected     */
    OWNET_SOCK_BOUND,      /* Local endpoint assigned                */
    OWNET_SOCK_CONNECTED   /* Peer assigned (TCP pseudo-connected)   */
} ownet_sock_state_t;

/* ── IPv4 endpoint ────────────────────────────────────────────── */

/* Addresses are host-order uint32_t; ports are host-order uint16_t. */
typedef struct {
    uint32_t addr;         /* IPv4 address in host byte order        */
    uint16_t port;         /* Port in host byte order                */
    uint16_t reserved;
} ownet_endpoint_t;

/* ── Pluggable link layer ─────────────────────────────────────── */

/* Transmit one frame. Return true on success. */
typedef bool (*ownet_link_tx_fn)(void *ctx,
                                 const uint8_t *frame,
                                 uint32_t len);
/* Poll for one inbound frame. Return the number of bytes copied into
 * frame (0 when no frame is available). */
typedef uint32_t (*ownet_link_rx_fn)(void *ctx,
                                     uint8_t *frame,
                                     uint32_t cap);

typedef struct {
    ownet_link_tx_fn   tx;
    ownet_link_rx_fn   rx;
    void              *ctx;      /* Opaque link-layer context          */
    uint32_t           mtu;      /* Negotiated MTU (<= OWNET_MTU)      */
} ownet_link_t;

/* ── Socket descriptor ────────────────────────────────────────── */

/* A socket record. For UDP the stack keeps a single pending inbound
 * datagram so recv() can serve it without heap. */
typedef struct {
    ownet_sock_state_t state;
    ownet_proto_t      proto;
    uint8_t            reserved[2];
    ownet_endpoint_t   local;
    ownet_endpoint_t   peer;
    uint32_t           tcp_seq;          /* Next TCP send sequence    */
    uint8_t            rxbuf[OWNET_MTU]; /* Pending inbound datagram  */
    uint32_t           rxlen;            /* Valid bytes in rxbuf      */
    ownet_endpoint_t   rxfrom;           /* Source of the pending datagram */
    bool               rx_ready;
} ownet_socket_t;

/* ── Stack context ────────────────────────────────────────────── */

typedef struct {
    bool            initialized;
    ownet_link_t    link;
    ownet_socket_t  sockets[OWNET_MAX_SOCKETS];
    uint32_t        live_sockets;
    uint16_t        next_port;   /* Ephemeral port allocator           */
    uint16_t        ip_id;       /* IPv4 identification counter        */
    uint64_t        tx_frames;
    uint64_t        rx_frames;
    uint64_t        tx_dropped;
} ownet_stack_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Initialize the stack and install a link-layer vtable. */
ownet_status_t ownet_init(ownet_stack_t *stack, const ownet_link_t *link);

/* Create a socket of the given protocol. Returns a handle through
 * *out_handle on success. */
ownet_status_t ownet_socket(ownet_stack_t *stack,
                            ownet_proto_t proto,
                            uint32_t *out_handle);

/* Bind a socket to a local endpoint. */
ownet_status_t ownet_bind(ownet_stack_t *stack,
                          uint32_t handle,
                          const ownet_endpoint_t *local);

/* Assign a peer endpoint, putting the socket in the connected state. */
ownet_status_t ownet_connect(ownet_stack_t *stack,
                             uint32_t handle,
                             const ownet_endpoint_t *peer);

/* Transmit a payload to the socket's peer (or to dst when given). */
ownet_status_t ownet_send(ownet_stack_t *stack,
                          uint32_t handle,
                          const ownet_endpoint_t *dst,
                          const void *data,
                          uint32_t len);

/* Receive one datagram into buf. Returns the byte count through
 * *out_len. Returns OWNET_ERR_WOULD_BLOCK when nothing is queued. */
ownet_status_t ownet_recv(ownet_stack_t *stack,
                          uint32_t handle,
                          void *buf,
                          uint32_t cap,
                          uint32_t *out_len);

/* Release a socket back to the pool. */
ownet_status_t ownet_close(ownet_stack_t *stack, uint32_t handle);

/* Poll the link once and enqueue any inbound datagram whose destination
 * matches a bound/connected socket. Returns the number of datagrams
 * delivered through *out_delivered. */
ownet_status_t ownet_poll(ownet_stack_t *stack, uint32_t *out_delivered);

#endif /* OWNET64_H */
