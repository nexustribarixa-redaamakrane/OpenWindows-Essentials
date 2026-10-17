/*
 * ownet64.c - OpenWindows Network Stack Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ownet64.h"

/* ── Protocol constants ───────────────────────────────────────── */

#define OWNET_IP_HDR_LEN     20u
#define OWNET_UDP_HDR_LEN     8u
#define OWNET_TCP_HDR_LEN    20u
#define OWNET_IPPROTO_TCP     6u
#define OWNET_IPPROTO_UDP    17u
#define OWNET_TCP_FIN      0x01u
#define OWNET_TCP_SYN      0x02u
#define OWNET_TCP_PSH      0x08u
#define OWNET_TCP_ACK      0x10u
#define OWNET_EPHEMERAL_LO 49152u
#define OWNET_POLL_BUDGET    16u

/* ── Internal helpers ─────────────────────────────────────────── */

static void ownet_memzero(void *dst, size_t n)
{
    uint8_t *d = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < n; ++i) d[i] = 0u;
}

static void ownet_memcopy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < n; ++i) d[i] = s[i];
}

static void ownet_put_be16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)(v & 0xFFu);
}

static void ownet_put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

static uint16_t ownet_get_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static uint32_t ownet_get_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

/* Incremental one's-complement checksum accumulator. Keeps a dangling
 * byte between updates so concatenated regions stay 16-bit aligned. */
typedef struct {
    uint32_t sum;
    uint8_t  pending;
    bool     has_pending;
} ownet_csum_t;

static void ownet_csum_init(ownet_csum_t *c)
{
    c->sum = 0u;
    c->pending = 0u;
    c->has_pending = false;
}

static void ownet_csum_update(ownet_csum_t *c, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; ++i) {
        if (c->has_pending) {
            c->sum += ((uint32_t)c->pending << 8) | (uint32_t)data[i];
            c->has_pending = false;
        } else {
            c->pending = data[i];
            c->has_pending = true;
        }
    }
}

static uint16_t ownet_csum_final(ownet_csum_t *c)
{
    uint32_t sum = c->sum;
    if (c->has_pending) sum += (uint32_t)c->pending << 8;
    while ((sum >> 16) != 0u) sum = (sum & 0xFFFFu) + (sum >> 16);
    return (uint16_t)(~sum & 0xFFFFu);
}

static ownet_status_t ownet_check(const ownet_stack_t *stack)
{
    if (!stack) return OWNET_ERR_PARAM;
    if (!stack->initialized) return OWNET_ERR_NOT_READY;
    return OWNET_OK;
}

static ownet_socket_t *ownet_get_socket(ownet_stack_t *stack, uint32_t handle)
{
    if (handle >= OWNET_MAX_SOCKETS) return NULL;
    if (stack->sockets[handle].state == OWNET_SOCK_CLOSED) return NULL;
    return &stack->sockets[handle];
}

static uint32_t ownet_link_mtu(const ownet_stack_t *stack)
{
    uint32_t mtu = stack->link.mtu;
    if (mtu == 0u || mtu > OWNET_MTU) mtu = OWNET_MTU;
    return mtu;
}

/* Round-robin ephemeral port allocator in the IANA dynamic range. */
static uint16_t ownet_alloc_port(ownet_stack_t *stack)
{
    uint16_t port = stack->next_port;
    if (port < OWNET_EPHEMERAL_LO) port = OWNET_EPHEMERAL_LO;
    stack->next_port = (uint16_t)(port + 1u);
    if (stack->next_port < OWNET_EPHEMERAL_LO) {
        stack->next_port = OWNET_EPHEMERAL_LO;
    }
    return port;
}

/* Serialize a 20-byte IPv4 header (no options) and fill its checksum. */
static void ownet_build_ip(uint8_t *h, uint16_t total, uint16_t id,
                           uint8_t ipproto, uint8_t ttl,
                           uint32_t src, uint32_t dst)
{
    ownet_csum_t csum;

    h[0] = 0x45u;                 /* version 4, IHL 5                    */
    h[1] = 0x00u;                 /* DSCP / ECN                          */
    ownet_put_be16(&h[2], total);
    ownet_put_be16(&h[4], id);
    ownet_put_be16(&h[6], 0x4000u); /* Don't Fragment                    */
    h[8] = ttl;
    h[9] = ipproto;
    ownet_put_be16(&h[10], 0u);
    ownet_put_be32(&h[12], src);
    ownet_put_be32(&h[16], dst);

    ownet_csum_init(&csum);
    ownet_csum_update(&csum, h, OWNET_IP_HDR_LEN);
    ownet_put_be16(&h[10], ownet_csum_final(&csum));
}

/* Build and transmit one frame. tcp_flags is ignored for UDP sockets. */
static ownet_status_t ownet_emit(ownet_stack_t *stack, ownet_socket_t *sk,
                                 const ownet_endpoint_t *dst,
                                 const void *data, uint32_t len,
                                 uint8_t tcp_flags)
{
    uint8_t  frame[OWNET_MTU];
    uint32_t mtu = ownet_link_mtu(stack);
    uint32_t hdr = (sk->proto == OWNET_PROTO_UDP) ? OWNET_UDP_HDR_LEN
                                                  : OWNET_TCP_HDR_LEN;
    uint32_t total;

    if (!dst) return OWNET_ERR_PARAM;
    if (len > mtu) return OWNET_ERR_TRUNCATED;

    total = OWNET_IP_HDR_LEN + hdr + len;
    if (total > mtu || total > OWNET_MTU) return OWNET_ERR_TRUNCATED;

    ownet_build_ip(frame, (uint16_t)total, stack->ip_id++,
                   (sk->proto == OWNET_PROTO_UDP) ? (uint8_t)OWNET_IPPROTO_UDP
                                                  : (uint8_t)OWNET_IPPROTO_TCP,
                   (uint8_t)OWNET_DEFAULT_TTL,
                   sk->local.addr, dst->addr);

    if (sk->proto == OWNET_PROTO_UDP) {
        uint8_t *u = &frame[OWNET_IP_HDR_LEN];
        ownet_put_be16(&u[0], sk->local.port);
        ownet_put_be16(&u[2], dst->port);
        ownet_put_be16(&u[4], (uint16_t)(OWNET_UDP_HDR_LEN + len));
        ownet_put_be16(&u[6], 0u); /* checksum optional for IPv4 UDP */
        if (len != 0u) ownet_memcopy(&u[OWNET_UDP_HDR_LEN], data, len);
    } else {
        uint8_t        pseudo[12];
        ownet_csum_t   csum;
        uint8_t       *t = &frame[OWNET_IP_HDR_LEN];
        uint32_t       seglen = OWNET_TCP_HDR_LEN + len;

        ownet_put_be16(&t[0], sk->local.port);
        ownet_put_be16(&t[2], dst->port);
        ownet_put_be32(&t[4], sk->tcp_seq);
        ownet_put_be32(&t[8], 0u);
        t[12] = 0x50u;            /* data offset 5 -> 20-byte header      */
        t[13] = tcp_flags;
        ownet_put_be16(&t[14], 4096u);
        ownet_put_be16(&t[16], 0u);
        ownet_put_be16(&t[18], 0u);
        if (len != 0u) ownet_memcopy(&t[OWNET_TCP_HDR_LEN], data, len);

        ownet_put_be32(&pseudo[0], sk->local.addr);
        ownet_put_be32(&pseudo[4], dst->addr);
        pseudo[8] = 0u;
        pseudo[9] = (uint8_t)OWNET_IPPROTO_TCP;
        ownet_put_be16(&pseudo[10], (uint16_t)seglen);

        ownet_csum_init(&csum);
        ownet_csum_update(&csum, pseudo, sizeof(pseudo));
        ownet_csum_update(&csum, t, seglen);
        ownet_put_be16(&t[16], ownet_csum_final(&csum));
    }

    if (stack->link.tx(stack->link.ctx, frame, total)) {
        stack->tx_frames++;
        return OWNET_OK;
    }
    stack->tx_dropped++;
    return OWNET_ERR_LINK;
}

/* ── Implementation ───────────────────────────────────────────── */

ownet_status_t ownet_init(ownet_stack_t *stack, const ownet_link_t *link)
{
    if (!stack || !link || !link->tx || !link->rx) return OWNET_ERR_PARAM;

    ownet_memzero(stack, sizeof(*stack));
    stack->link = *link;
    if (stack->link.mtu == 0u || stack->link.mtu > OWNET_MTU) {
        stack->link.mtu = OWNET_MTU;
    }
    stack->next_port = OWNET_EPHEMERAL_LO;
    stack->ip_id = 1u;
    stack->initialized = true;
    return OWNET_OK;
}

ownet_status_t ownet_socket(ownet_stack_t *stack,
                            ownet_proto_t proto,
                            uint32_t *out_handle)
{
    ownet_status_t st = ownet_check(stack);
    uint32_t i;

    if (st != OWNET_OK) return st;
    if (!out_handle) return OWNET_ERR_PARAM;
    if (proto != OWNET_PROTO_UDP && proto != OWNET_PROTO_TCP) {
        return OWNET_ERR_PARAM;
    }

    for (i = 0u; i < OWNET_MAX_SOCKETS; ++i) {
        ownet_socket_t *sk = &stack->sockets[i];
        if (sk->state == OWNET_SOCK_CLOSED) {
            ownet_memzero(sk, sizeof(*sk));
            sk->state = OWNET_SOCK_OPENED;
            sk->proto = proto;
            sk->tcp_seq = 0x4F570001u + i; /* deterministic initial seq  */
            stack->live_sockets++;
            *out_handle = i;
            return OWNET_OK;
        }
    }
    return OWNET_ERR_NO_MEM;
}

ownet_status_t ownet_bind(ownet_stack_t *stack,
                          uint32_t handle,
                          const ownet_endpoint_t *local)
{
    ownet_status_t st = ownet_check(stack);
    ownet_socket_t *sk;
    uint32_t i;

    if (st != OWNET_OK) return st;
    if (!local) return OWNET_ERR_PARAM;

    sk = ownet_get_socket(stack, handle);
    if (!sk) return OWNET_ERR_NOT_FOUND;
    if (sk->state == OWNET_SOCK_CONNECTED) return OWNET_ERR_STATE;

    /* Reject duplicate local endpoints; wildcard binds may overlap. */
    if (local->port != 0u) {
        for (i = 0u; i < OWNET_MAX_SOCKETS; ++i) {
            ownet_socket_t *o = &stack->sockets[i];
            if (o == sk || o->state == OWNET_SOCK_CLOSED) continue;
            if (o->local.port != local->port) continue;
            if (o->local.addr == 0u || local->addr == 0u ||
                o->local.addr == local->addr) {
                return OWNET_ERR_ADDR_IN_USE;
            }
        }
    }

    sk->local = *local;
    sk->state = OWNET_SOCK_BOUND;
    return OWNET_OK;
}

ownet_status_t ownet_connect(ownet_stack_t *stack,
                             uint32_t handle,
                             const ownet_endpoint_t *peer)
{
    ownet_status_t st = ownet_check(stack);
    ownet_socket_t *sk;

    if (st != OWNET_OK) return st;
    if (!peer || peer->port == 0u) return OWNET_ERR_PARAM;

    sk = ownet_get_socket(stack, handle);
    if (!sk) return OWNET_ERR_NOT_FOUND;
    if (sk->state == OWNET_SOCK_CONNECTED) return OWNET_ERR_STATE;

    /* Implicit bind to an ephemeral port if none was requested. */
    if (sk->local.port == 0u) {
        sk->local.port = ownet_alloc_port(stack);
    }
    sk->peer = *peer;
    sk->state = OWNET_SOCK_CONNECTED;

    if (sk->proto == OWNET_PROTO_TCP) {
        st = ownet_emit(stack, sk, peer, NULL, 0u, OWNET_TCP_SYN);
        if (st != OWNET_OK) return st;
        sk->tcp_seq += 1u;        /* SYN consumes one sequence number     */
    }
    return OWNET_OK;
}

ownet_status_t ownet_send(ownet_stack_t *stack,
                          uint32_t handle,
                          const ownet_endpoint_t *dst,
                          const void *data,
                          uint32_t len)
{
    ownet_status_t st = ownet_check(stack);
    ownet_socket_t *sk;
    const ownet_endpoint_t *target;
    uint8_t flags = 0u;

    if (st != OWNET_OK) return st;
    if (len != 0u && !data) return OWNET_ERR_PARAM;

    sk = ownet_get_socket(stack, handle);
    if (!sk) return OWNET_ERR_NOT_FOUND;

    if (sk->state == OWNET_SOCK_OPENED) {
        sk->local.port = ownet_alloc_port(stack);
        sk->state = OWNET_SOCK_BOUND;
    }

    target = dst;
    if (!target && sk->state == OWNET_SOCK_CONNECTED) target = &sk->peer;
    if (!target) return OWNET_ERR_STATE;
    if (sk->proto == OWNET_PROTO_TCP && sk->state != OWNET_SOCK_CONNECTED) {
        return OWNET_ERR_STATE;
    }

    if (sk->proto == OWNET_PROTO_TCP) flags = OWNET_TCP_PSH | OWNET_TCP_ACK;

    st = ownet_emit(stack, sk, target, data, len, flags);
    if (st == OWNET_OK && sk->proto == OWNET_PROTO_TCP) {
        sk->tcp_seq += len;
    }
    return st;
}

ownet_status_t ownet_recv(ownet_stack_t *stack,
                          uint32_t handle,
                          void *buf,
                          uint32_t cap,
                          uint32_t *out_len)
{
    ownet_status_t st = ownet_check(stack);
    ownet_socket_t *sk;

    if (st != OWNET_OK) return st;
    if (!buf || !out_len) return OWNET_ERR_PARAM;

    sk = ownet_get_socket(stack, handle);
    if (!sk) return OWNET_ERR_NOT_FOUND;
    if (sk->state != OWNET_SOCK_BOUND && sk->state != OWNET_SOCK_CONNECTED) {
        return OWNET_ERR_STATE;
    }

    *out_len = 0u;
    if (!sk->rx_ready) return OWNET_ERR_WOULD_BLOCK;
    if (cap < sk->rxlen) return OWNET_ERR_TRUNCATED;

    ownet_memcopy(buf, sk->rxbuf, sk->rxlen);
    *out_len = sk->rxlen;
    sk->rxlen = 0u;
    sk->rx_ready = false;
    return OWNET_OK;
}

ownet_status_t ownet_close(ownet_stack_t *stack, uint32_t handle)
{
    ownet_status_t st = ownet_check(stack);
    ownet_socket_t *sk;

    if (st != OWNET_OK) return st;

    sk = ownet_get_socket(stack, handle);
    if (!sk) return OWNET_ERR_NOT_FOUND;

    if (sk->proto == OWNET_PROTO_TCP && sk->state == OWNET_SOCK_CONNECTED) {
        (void)ownet_emit(stack, sk, &sk->peer, NULL, 0u,
                         OWNET_TCP_FIN | OWNET_TCP_ACK);
        sk->tcp_seq += 1u;        /* FIN consumes one sequence number     */
    }

    ownet_memzero(sk, sizeof(*sk));
    sk->state = OWNET_SOCK_CLOSED;
    if (stack->live_sockets > 0u) stack->live_sockets--;
    return OWNET_OK;
}

ownet_status_t ownet_poll(ownet_stack_t *stack, uint32_t *out_delivered)
{
    ownet_status_t st = ownet_check(stack);
    uint32_t delivered = 0u;
    uint32_t iter;

    if (st != OWNET_OK) return st;
    if (!stack->link.rx) return OWNET_ERR_NOT_READY;

    for (iter = 0u; iter < OWNET_POLL_BUDGET; ++iter) {
        uint8_t       frame[OWNET_MTU];
        uint32_t      got = stack->link.rx(stack->link.ctx, frame, OWNET_MTU);
        uint32_t      ihl;
        uint32_t      total;
        uint8_t       ipproto;
        uint32_t      src;
        uint32_t      dst;
        const uint8_t *seg;
        uint32_t      seglen;
        uint16_t      sport = 0u;
        uint16_t      dport = 0u;
        const uint8_t *payload = NULL;
        uint32_t      paylen = 0u;
        ownet_proto_t sproto;
        ownet_socket_t *sk = NULL;
        uint32_t      i;

        if (got == 0u) break;
        if (got < OWNET_IP_HDR_LEN) continue;
        if ((frame[0] >> 4) != 4u) continue;

        ihl = (uint32_t)(frame[0] & 0x0Fu) * 4u;
        if (ihl < OWNET_IP_HDR_LEN || ihl > got) continue;

        total = (uint32_t)ownet_get_be16(&frame[2]);
        if (total < ihl || total > got) total = got;

        ipproto = frame[9];
        src = ownet_get_be32(&frame[12]);
        dst = ownet_get_be32(&frame[16]);
        stack->rx_frames++;

        seg = &frame[ihl];
        seglen = total - ihl;

        if (ipproto == OWNET_IPPROTO_UDP) {
            uint32_t ulen;
            if (seglen < OWNET_UDP_HDR_LEN) continue;
            sport = ownet_get_be16(&seg[0]);
            dport = ownet_get_be16(&seg[2]);
            ulen = (uint32_t)ownet_get_be16(&seg[4]);
            if (ulen < OWNET_UDP_HDR_LEN) continue;
            paylen = ulen - OWNET_UDP_HDR_LEN;
            if (paylen > seglen - OWNET_UDP_HDR_LEN) {
                paylen = seglen - OWNET_UDP_HDR_LEN;
            }
            payload = &seg[OWNET_UDP_HDR_LEN];
            sproto = OWNET_PROTO_UDP;
        } else if (ipproto == OWNET_IPPROTO_TCP) {
            uint32_t doff;
            if (seglen < OWNET_TCP_HDR_LEN) continue;
            sport = ownet_get_be16(&seg[0]);
            dport = ownet_get_be16(&seg[2]);
            doff = (uint32_t)(seg[12] >> 4) * 4u;
            if (doff < OWNET_TCP_HDR_LEN || doff > seglen) continue;
            payload = &seg[doff];
            paylen = seglen - doff;
            sproto = OWNET_PROTO_TCP;
        } else {
            continue;
        }

        for (i = 0u; i < OWNET_MAX_SOCKETS; ++i) {
            ownet_socket_t *c = &stack->sockets[i];
            if (c->state != OWNET_SOCK_BOUND &&
                c->state != OWNET_SOCK_CONNECTED) continue;
            if (c->proto != sproto) continue;
            if (c->local.port != dport) continue;
            if (c->local.addr != 0u && c->local.addr != dst) continue;
            sk = c;
            break;
        }
        if (!sk) continue;
        if (sk->rx_ready) continue; /* previous datagram still pending    */

        if (paylen > OWNET_MTU) paylen = OWNET_MTU;
        if (paylen != 0u) ownet_memcopy(sk->rxbuf, payload, paylen);
        sk->rxlen = paylen;
        sk->rxfrom.addr = src;
        sk->rxfrom.port = sport;
        sk->rxfrom.reserved = 0u;
        sk->rx_ready = true;
        delivered++;
    }

    if (out_delivered) *out_delivered = delivered;
    return OWNET_OK;
}
