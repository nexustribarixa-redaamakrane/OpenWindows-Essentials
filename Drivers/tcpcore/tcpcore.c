/*
 * tcpcore.c - OpenWindows TCP Transport Core (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "tcpcore.h"

/* ── Local helpers ────────────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

static void ow_memcpy(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static inline uint16_t ow_bswap16(uint16_t v)
{
    return (uint16_t)((uint16_t)(v << 8) | (uint16_t)(v >> 8));
}

static inline uint32_t ow_bswap32(uint32_t v)
{
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

/* ── Sequence arithmetic (wrap-aware) ─────────────────────────── */

static inline bool tcp_seq_lt(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) < 0;
}

static inline bool tcp_seq_le(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) <= 0;
}

static inline bool tcp_seq_gt(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) > 0;
}

static inline bool tcp_seq_ge(uint32_t a, uint32_t b)
{
    return (int32_t)(a - b) >= 0;
}

/* ── Checksum (IPv4 pseudo-header + segment) ──────────────────── */

/* Fold the two big-endian 16-bit words of a stored address into sum.
 * The stored address bytes are the on-wire bytes, so this is correct
 * regardless of host endianness. */
static uint32_t tcp_sum_addr(uint32_t sum, uint32_t addr)
{
    const uint8_t *p = (const uint8_t *)&addr;
    sum += (uint32_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
    sum += (uint32_t)(((uint16_t)p[2] << 8) | (uint16_t)p[3]);
    return sum;
}

static uint16_t tcp_checksum(uint32_t src, uint32_t dst,
                             const uint8_t *seg, uint32_t len)
{
    uint32_t sum = 0u;
    uint32_t i;

    sum = tcp_sum_addr(sum, src);
    sum = tcp_sum_addr(sum, dst);
    sum += (uint32_t)TCP_PROTO;
    sum += len;

    for (i = 0u; (i + 1u) < len; i += 2u) {
        sum += (uint32_t)(((uint16_t)seg[i] << 8) | (uint16_t)seg[i + 1u]);
    }
    if (i < len) {
        sum += (uint32_t)((uint16_t)seg[i] << 8);
    }
    while ((sum >> 16) != 0u) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

/* ── Connection table helpers ─────────────────────────────────── */

static tcp_conn_t *tcp_conn_at(tcpcore_t *core, tcp_conn_id_t id)
{
    if (id <= 0 || (uint32_t)id > TCP_MAX_CONNS) {
        return NULL;
    }
    if (!core->conns[(uint32_t)id - 1u].used) {
        return NULL;
    }
    return &core->conns[(uint32_t)id - 1u];
}

static tcp_conn_id_t tcp_conn_id(const tcpcore_t *core,
                                 const tcp_conn_t *conn)
{
    size_t idx = (size_t)(conn - core->conns);
    return (tcp_conn_id_t)(idx + 1u);
}

static tcp_conn_t *tcp_alloc_conn(tcpcore_t *core)
{
    uint32_t i;
    for (i = 0u; i < TCP_MAX_CONNS; ++i) {
        if (!core->conns[i].used) {
            return &core->conns[i];
        }
    }
    return NULL;
}

static tcp_conn_t *tcp_find(tcpcore_t *core, uint32_t remote_ip,
                            uint16_t remote_port, uint32_t local_ip,
                            uint16_t local_port)
{
    uint32_t i;
    for (i = 0u; i < TCP_MAX_CONNS; ++i) {
        tcp_conn_t *c = &core->conns[i];
        if (!c->used) {
            continue;
        }
        if (c->remote_ip != remote_ip || c->remote_port != remote_port ||
            c->local_port != local_port) {
            continue;
        }
        if (c->local_ip != TCP_IP_ADDR_ANY && c->local_ip != local_ip) {
            continue;
        }
        return c;
    }
    return NULL;
}

static tcp_listener_t *tcp_find_listener(tcpcore_t *core, uint16_t port)
{
    uint32_t i;
    for (i = 0u; i < TCP_MAX_LISTENERS; ++i) {
        if (core->listeners[i].used &&
            core->listeners[i].local_port == port) {
            return &core->listeners[i];
        }
    }
    return NULL;
}

/* Release a connection; keep the backlog count consistent. */
static void tcp_release(tcpcore_t *core, tcp_conn_t *conn)
{
    if (conn->passive && !conn->accepted) {
        tcp_listener_t *l = tcp_find_listener(core, conn->local_port);
        if (l != NULL && l->pending > 0u) {
            l->pending--;
        }
    }
    ow_memset(conn, 0, sizeof(*conn));
}

/* ── Pseudo-random initial sequence number ────────────────────── */

static uint32_t tcp_next_isn(tcpcore_t *core)
{
    core->iss_seed = (uint16_t)((core->iss_seed * 25173u) + 13849u);
    return ((uint32_t)core->iss_seed << 16) ^
           (uint32_t)(core->iss_seed * 40503u);
}

/* ── Segment transmit ─────────────────────────────────────────── */

/* Build and send a segment. When save is set the segment is retained
 * for retransmission (it consumes sequence space and expects an ACK). */
static tcp_status_t tcp_emit(tcpcore_t *core, tcp_conn_t *conn,
                             uint8_t flags, const void *payload,
                             uint32_t len, bool save)
{
    tcp_header_t *h;
    uint32_t seg_len;
    uint16_t csum;
    uint32_t seq_start;

    if (len > TCP_MSS) {
        return TCP_ERR_NO_SPACE;
    }
    seg_len = TCP_HDR_LEN + len;
    seq_start = conn->snd_nxt;

    h = (tcp_header_t *)(void *)core->scratch;
    h->src_port    = ow_bswap16(conn->local_port);
    h->dst_port    = ow_bswap16(conn->remote_port);
    h->seq         = ow_bswap32(seq_start);
    h->ack         = ow_bswap32(conn->rcv_nxt);
    h->data_offset = (uint8_t)((TCP_HDR_LEN / 4u) << 4);
    h->flags       = flags;
    h->window      = ow_bswap16((uint16_t)conn->rcv_wnd);
    h->checksum    = 0u;
    h->urgent      = 0u;

    if (len != 0u) {
        ow_memcpy(core->scratch + TCP_HDR_LEN, payload, len);
    }
    csum = tcp_checksum(conn->local_ip, conn->remote_ip, core->scratch,
                        seg_len);
    if (csum == 0u) {
        csum = 0xFFFFu;   /* zero would be indistinguishable from absent */
    }
    h->checksum = ow_bswap16(csum);

    /* Advance the send sequence by payload, SYN and FIN. */
    conn->snd_nxt += len;
    if ((flags & TCP_SYN) != 0u) {
        conn->snd_nxt += 1u;
    }
    if ((flags & TCP_FIN) != 0u) {
        conn->snd_nxt += 1u;
    }

    if (save) {
        ow_memcpy(conn->tx_buf, core->scratch, seg_len);
        conn->tx_len      = seg_len;
        conn->tx_seq      = seq_start;
        conn->tx_end_seq  = conn->snd_nxt;
        conn->tx_pending  = true;
        conn->rto         = TCP_INITIAL_RTO;
        conn->rto_timer   = TCP_INITIAL_RTO;
    }

    return core->ip_tx(core->ip_ctx, conn->local_ip, conn->remote_ip,
                       core->scratch, seg_len);
}

/* Emit a stateless RST for a segment that matches no connection. */
static void tcp_send_rst(tcpcore_t *core, uint32_t src_ip, uint16_t src_port,
                         uint32_t dst_ip, uint16_t dst_port, uint32_t seq,
                         bool with_ack, uint32_t ack)
{
    tcp_header_t *h = (tcp_header_t *)(void *)core->scratch;
    uint16_t csum;

    h->src_port    = ow_bswap16(src_port);
    h->dst_port    = ow_bswap16(dst_port);
    h->seq         = ow_bswap32(seq);
    h->ack         = ow_bswap32(ack);
    h->data_offset = (uint8_t)((TCP_HDR_LEN / 4u) << 4);
    h->flags       = (uint8_t)(TCP_RST | (with_ack ? TCP_ACK : 0u));
    h->window      = 0u;
    h->checksum    = 0u;
    h->urgent      = 0u;

    csum = tcp_checksum(src_ip, dst_ip, core->scratch, TCP_HDR_LEN);
    if (csum == 0u) {
        csum = 0xFFFFu;
    }
    h->checksum = ow_bswap16(csum);

    (void)core->ip_tx(core->ip_ctx, src_ip, dst_ip, core->scratch,
                      TCP_HDR_LEN);
}

/* ── Passive open ─────────────────────────────────────────────── */

static tcp_status_t tcp_passive_open(tcpcore_t *core, uint32_t local_ip,
                                     uint16_t local_port, uint32_t remote_ip,
                                     uint16_t remote_port, uint32_t seq,
                                     uint16_t window)
{
    tcp_listener_t *l = tcp_find_listener(core, local_port);
    tcp_conn_t *c;

    if (l == NULL) {
        return TCP_ERR_NO_CONN;
    }
    if (l->pending >= l->backlog) {
        return TCP_ERR_FULL;
    }
    c = tcp_alloc_conn(core);
    if (c == NULL) {
        return TCP_ERR_FULL;
    }

    ow_memset(c, 0, sizeof(*c));
    c->used        = true;
    c->passive     = true;
    c->accepted    = false;
    c->state       = TCP_STATE_SYN_RCVD;
    c->local_ip    = local_ip;
    c->remote_ip   = remote_ip;
    c->local_port  = local_port;
    c->remote_port = remote_port;
    c->iss         = tcp_next_isn(core);
    c->irs         = seq;
    c->snd_una     = c->iss;
    c->snd_nxt     = c->iss;
    c->rcv_nxt     = seq + 1u;
    c->rcv_wnd     = 4096u;
    c->snd_wnd     = window;
    c->mss         = (uint16_t)TCP_MSS;
    c->rto         = TCP_INITIAL_RTO;
    l->pending++;

    return tcp_emit(core, c, (uint8_t)(TCP_SYN | TCP_ACK), NULL, 0u, true);
}

/* ── Public API ───────────────────────────────────────────────── */

tcp_status_t tcp_init(tcpcore_t *core, uint32_t local_ip,
                      tcp_ip_tx_fn ip_tx, void *ip_ctx)
{
    if (core == NULL || ip_tx == NULL) {
        return TCP_ERR_PARAM;
    }
    ow_memset(core, 0, sizeof(*core));
    core->local_ip   = local_ip;
    core->ip_tx      = ip_tx;
    core->ip_ctx     = ip_ctx;
    core->iss_seed   = 0x1D2Bu;
    core->initialized = true;
    return TCP_OK;
}

tcp_conn_id_t tcp_connect(tcpcore_t *core, uint32_t local_ip,
                          uint16_t local_port, uint32_t remote_ip,
                          uint16_t remote_port)
{
    tcp_conn_t *c;

    if (core == NULL || !core->initialized) {
        return 0;
    }
    if (local_port == 0u || remote_port == 0u || remote_ip == 0u) {
        return 0;
    }
    if (tcp_find(core, remote_ip, remote_port, local_ip, local_port) != NULL) {
        return 0;
    }

    c = tcp_alloc_conn(core);
    if (c == NULL) {
        return 0;
    }

    ow_memset(c, 0, sizeof(*c));
    c->used        = true;
    c->passive     = false;
    c->state       = TCP_STATE_SYN_SENT;
    c->local_ip    = local_ip;
    c->remote_ip   = remote_ip;
    c->local_port  = local_port;
    c->remote_port = remote_port;
    c->iss         = tcp_next_isn(core);
    c->snd_una     = c->iss;
    c->snd_nxt     = c->iss;
    c->rcv_nxt     = 0u;
    c->rcv_wnd     = 4096u;
    c->snd_wnd     = 4096u;
    c->mss         = (uint16_t)TCP_MSS;
    c->rto         = TCP_INITIAL_RTO;
    c->rto_timer   = TCP_INITIAL_RTO;

    if (tcp_emit(core, c, TCP_SYN, NULL, 0u, true) != TCP_OK) {
        ow_memset(c, 0, sizeof(*c));
        return 0;
    }
    return tcp_conn_id(core, c);
}

tcp_status_t tcp_listen(tcpcore_t *core, uint32_t local_ip,
                        uint16_t local_port, uint16_t backlog)
{
    uint32_t i;

    if (core == NULL || !core->initialized || local_port == 0u) {
        return TCP_ERR_PARAM;
    }
    if (backlog == 0u) {
        backlog = 1u;
    }

    for (i = 0u; i < TCP_MAX_LISTENERS; ++i) {
        if (core->listeners[i].used &&
            core->listeners[i].local_port == local_port) {
            core->listeners[i].backlog = backlog;
            return TCP_OK;
        }
    }
    for (i = 0u; i < TCP_MAX_LISTENERS; ++i) {
        if (!core->listeners[i].used) {
            core->listeners[i].used       = true;
            core->listeners[i].local_ip   = local_ip;
            core->listeners[i].local_port = local_port;
            core->listeners[i].backlog    = backlog;
            core->listeners[i].pending    = 0u;
            return TCP_OK;
        }
    }
    return TCP_ERR_FULL;
}

tcp_status_t tcp_accept(tcpcore_t *core, uint32_t local_ip,
                        uint16_t local_port, tcp_conn_id_t *out_conn)
{
    uint32_t i;

    if (core == NULL || out_conn == NULL) {
        return TCP_ERR_PARAM;
    }
    for (i = 0u; i < TCP_MAX_CONNS; ++i) {
        tcp_conn_t *c = &core->conns[i];
        if (!c->used || !c->passive || c->accepted) {
            continue;
        }
        if (c->state != TCP_STATE_ESTABLISHED) {
            continue;
        }
        if (c->local_port != local_port) {
            continue;
        }
        if (c->local_ip != TCP_IP_ADDR_ANY && c->local_ip != local_ip) {
            continue;
        }
        c->accepted = true;
        {
            tcp_listener_t *l = tcp_find_listener(core, local_port);
            if (l != NULL && l->pending > 0u) {
                l->pending--;
            }
        }
        *out_conn = (tcp_conn_id_t)(i + 1u);
        return TCP_OK;
    }
    return TCP_ERR_AGAIN;
}

tcp_status_t tcp_send(tcpcore_t *core, tcp_conn_id_t conn,
                      const void *data, uint32_t len)
{
    tcp_conn_t *c;

    if (core == NULL || (data == NULL && len != 0u)) {
        return TCP_ERR_PARAM;
    }
    c = tcp_conn_at(core, conn);
    if (c == NULL) {
        return TCP_ERR_NO_CONN;
    }
    if (c->state != TCP_STATE_ESTABLISHED &&
        c->state != TCP_STATE_CLOSE_WAIT) {
        return TCP_ERR_CLOSED;
    }
    if (len == 0u) {
        return TCP_OK;
    }
    if (len > TCP_MSS) {
        return TCP_ERR_NO_SPACE;
    }
    if (c->tx_pending) {
        return TCP_ERR_AGAIN;   /* previous segment not yet ACKed */
    }
    return tcp_emit(core, c, (uint8_t)(TCP_ACK | TCP_PSH), data, len, true);
}

tcp_status_t tcp_recv(tcpcore_t *core, tcp_conn_id_t conn, void *buf,
                      uint32_t cap, uint32_t *out_len)
{
    tcp_conn_t *c;
    uint32_t n;
    uint32_t i;

    if (core == NULL || buf == NULL || out_len == NULL) {
        return TCP_ERR_PARAM;
    }
    c = tcp_conn_at(core, conn);
    if (c == NULL) {
        return TCP_ERR_NO_CONN;
    }
    if (c->rx_len == 0u) {
        if (c->state == TCP_STATE_CLOSED) {
            return TCP_ERR_CLOSED;
        }
        return TCP_ERR_AGAIN;
    }

    n = (c->rx_len < cap) ? c->rx_len : cap;
    ow_memcpy(buf, c->rx_buf, n);
    for (i = n; i < c->rx_len; ++i) {
        c->rx_buf[i - n] = c->rx_buf[i];
    }
    c->rx_len -= n;
    *out_len = n;
    return TCP_OK;
}

tcp_status_t tcp_close(tcpcore_t *core, tcp_conn_id_t conn)
{
    tcp_conn_t *c;

    if (core == NULL) {
        return TCP_ERR_PARAM;
    }
    c = tcp_conn_at(core, conn);
    if (c == NULL) {
        return TCP_ERR_NO_CONN;
    }

    switch (c->state) {
    case TCP_STATE_ESTABLISHED:
        c->state = TCP_STATE_FIN_WAIT_1;
        return tcp_emit(core, c, (uint8_t)(TCP_FIN | TCP_ACK), NULL, 0u,
                        true);
    case TCP_STATE_CLOSE_WAIT:
        c->state = TCP_STATE_LAST_ACK;
        return tcp_emit(core, c, (uint8_t)(TCP_FIN | TCP_ACK), NULL, 0u,
                        true);
    case TCP_STATE_SYN_SENT:
    case TCP_STATE_SYN_RCVD:
        /* Abort before the connection is established. */
        tcp_release(core, c);
        return TCP_OK;
    case TCP_STATE_CLOSED:
        return TCP_ERR_CLOSED;
    default:
        return TCP_ERR_AGAIN;   /* close already in progress */
    }
}

/* ── Inbound segment processing ───────────────────────────────── */

tcp_status_t tcp_input(tcpcore_t *core, uint32_t src_ip, uint32_t dst_ip,
                       const uint8_t *segment, uint32_t len)
{
    const tcp_header_t *h;
    uint16_t sport;
    uint16_t dport;
    uint32_t seq;
    uint32_t ack;
    uint8_t  flags;
    uint32_t doff;
    uint32_t payload_len;
    const uint8_t *payload;
    uint16_t window;
    tcp_conn_t *c;

    if (core == NULL || segment == NULL) {
        return TCP_ERR_PARAM;
    }
    if (!core->initialized) {
        return TCP_ERR_NOT_READY;
    }
    if (len < TCP_HDR_LEN) {
        return TCP_ERR_BAD_PACKET;
    }

    h = (const tcp_header_t *)(const void *)segment;
    sport = ow_bswap16(h->src_port);
    dport = ow_bswap16(h->dst_port);
    seq   = ow_bswap32(h->seq);
    ack   = ow_bswap32(h->ack);
    flags = h->flags;
    window = ow_bswap16(h->window);
    doff = (uint32_t)(h->data_offset >> 4) * 4u;
    if (doff < TCP_HDR_LEN || doff > len) {
        return TCP_ERR_BAD_PACKET;
    }
    payload_len = len - doff;
    payload = segment + doff;

    if (tcp_checksum(src_ip, dst_ip, segment, len) != 0u) {
        return TCP_ERR_BAD_PACKET;
    }

    c = tcp_find(core, src_ip, sport, dst_ip, dport);
    if (c == NULL) {
        if (((flags & TCP_SYN) != 0u) && ((flags & TCP_ACK) == 0u)) {
            return tcp_passive_open(core, dst_ip, dport, src_ip, sport,
                                    seq, window);
        }
        if ((flags & TCP_RST) == 0u) {
            uint32_t rseq = seq + payload_len;
            if ((flags & TCP_SYN) != 0u) {
                rseq += 1u;
            }
            if ((flags & TCP_FIN) != 0u) {
                rseq += 1u;
            }
            tcp_send_rst(core, dst_ip, dport, src_ip, sport, rseq,
                         (flags & TCP_ACK) != 0u, ack);
        }
        return TCP_ERR_NO_CONN;
    }

    /* Reset tears the connection down unconditionally. */
    if ((flags & TCP_RST) != 0u) {
        tcp_release(core, c);
        return TCP_OK;
    }

    /* ACK processing. */
    if ((flags & TCP_ACK) != 0u) {
        if (c->state == TCP_STATE_SYN_SENT) {
            /* The SYN-ACK must acknowledge our ISS exactly. */
            if (ack != (c->iss + 1u)) {
                return TCP_ERR_BAD_PACKET;
            }
            c->snd_una = ack;
            c->snd_wnd = window;
            c->irs     = seq;
            c->rcv_nxt = seq + 1u;
            c->state   = TCP_STATE_ESTABLISHED;
            c->tx_pending = false;
            return tcp_emit(core, c, TCP_ACK, NULL, 0u, false);
        }

        if (c->state == TCP_STATE_SYN_RCVD) {
            if (ack != c->snd_nxt) {
                return TCP_ERR_BAD_PACKET;
            }
            c->snd_una = ack;
            c->state   = TCP_STATE_ESTABLISHED;
            c->tx_pending = false;
        } else if (tcp_seq_gt(ack, c->snd_una) &&
                   tcp_seq_le(ack, c->snd_nxt)) {
            c->snd_una = ack;
            c->snd_wnd = window;
            if (c->tx_pending && tcp_seq_ge(c->snd_una, c->tx_end_seq)) {
                c->tx_pending = false;
            }
        }
    }

    /* In-order data delivery (only while the receive path is open). */
    if (c->state == TCP_STATE_ESTABLISHED ||
        c->state == TCP_STATE_FIN_WAIT_1 ||
        c->state == TCP_STATE_FIN_WAIT_2) {
        if (payload_len != 0u) {
            if (seq == c->rcv_nxt &&
                (c->rx_len + payload_len) <= TCP_RXBUF_SIZE) {
                ow_memcpy(c->rx_buf + c->rx_len, payload, payload_len);
                c->rx_len += payload_len;
                c->rcv_nxt += payload_len;
            }
            /* Always acknowledge; out-of-order data is discarded. */
            if (tcp_emit(core, c, TCP_ACK, NULL, 0u, false) != TCP_OK) {
                return TCP_ERR_AGAIN;
            }
        }

        if ((flags & TCP_FIN) != 0u) {
            c->rcv_nxt = seq + payload_len + 1u;
            (void)tcp_emit(core, c, TCP_ACK, NULL, 0u, false);
            if (c->state == TCP_STATE_ESTABLISHED) {
                c->state = TCP_STATE_CLOSE_WAIT;
            } else if (c->state == TCP_STATE_FIN_WAIT_1) {
                c->state = TCP_STATE_CLOSING;
            } else { /* FIN_WAIT_2 */
                c->state = TCP_STATE_TIME_WAIT;
                c->time_wait_timer = TCP_TIME_WAIT_TICKS;
            }
        }
    }

    /* Completion of our own close sequence. */
    if (c->state == TCP_STATE_FIN_WAIT_1 &&
        tcp_seq_ge(c->snd_una, c->snd_nxt)) {
        c->state = TCP_STATE_FIN_WAIT_2;
    }
    if (c->state == TCP_STATE_CLOSING &&
        tcp_seq_ge(c->snd_una, c->snd_nxt)) {
        c->state = TCP_STATE_TIME_WAIT;
        c->time_wait_timer = TCP_TIME_WAIT_TICKS;
    }
    if (c->state == TCP_STATE_LAST_ACK &&
        tcp_seq_ge(c->snd_una, c->snd_nxt)) {
        tcp_release(core, c);
    }

    return TCP_OK;
}

/* ── Timers ───────────────────────────────────────────────────── */

tcp_status_t tcp_tick(tcpcore_t *core, uint32_t elapsed_ticks)
{
    uint32_t i;

    if (core == NULL) {
        return TCP_ERR_PARAM;
    }
    if (!core->initialized) {
        return TCP_ERR_NOT_READY;
    }

    for (i = 0u; i < TCP_MAX_CONNS; ++i) {
        tcp_conn_t *c = &core->conns[i];
        if (!c->used) {
            continue;
        }

        if (c->state == TCP_STATE_TIME_WAIT) {
            if (c->time_wait_timer <= elapsed_ticks) {
                tcp_release(core, c);
            } else {
                c->time_wait_timer -= elapsed_ticks;
            }
            continue;
        }

        if (!c->tx_pending) {
            continue;
        }
        if (c->rto_timer <= elapsed_ticks) {
            (void)core->ip_tx(core->ip_ctx, c->local_ip, c->remote_ip,
                              c->tx_buf, c->tx_len);
            c->rto *= 2u;
            if (c->rto > TCP_MAX_RTO) {
                c->rto = TCP_MAX_RTO;
            }
            c->rto_timer = c->rto;
        } else {
            c->rto_timer -= elapsed_ticks;
        }
    }
    return TCP_OK;
}

tcp_state_t tcp_conn_state(const tcpcore_t *core, tcp_conn_id_t conn)
{
    if (core == NULL || conn <= 0 || (uint32_t)conn > TCP_MAX_CONNS) {
        return TCP_STATE_CLOSED;
    }
    if (!core->conns[(uint32_t)conn - 1u].used) {
        return TCP_STATE_CLOSED;
    }
    return core->conns[(uint32_t)conn - 1u].state;
}
