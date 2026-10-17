/*
 * udpcore.c - OpenWindows UDP Transport Core (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "udpcore.h"

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

/* ── Checksum ─────────────────────────────────────────────────── */

/* Fold the two big-endian 16-bit words of a stored address into sum.
 * The stored address bytes are the on-wire bytes, so this is correct
 * regardless of host endianness. */
static uint32_t udp_sum_addr(uint32_t sum, uint32_t addr)
{
    const uint8_t *p = (const uint8_t *)&addr;
    sum += (uint32_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
    sum += (uint32_t)(((uint16_t)p[2] << 8) | (uint16_t)p[3]);
    return sum;
}

/* One's-complement sum of the IPv4 pseudo-header plus the datagram.
 * Returns the raw complement; a valid datagram sums to zero. */
static uint16_t udp_checksum(uint32_t src, uint32_t dst,
                             const uint8_t *seg, uint32_t len)
{
    uint32_t sum = 0u;
    uint32_t i;

    sum = udp_sum_addr(sum, src);
    sum = udp_sum_addr(sum, dst);
    sum += (uint32_t)UDP_PROTO;
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

/* Encoded checksum for transmission: zero means "absent", so emit 0xFFFF. */
static uint16_t udp_checksum_encode(uint32_t src, uint32_t dst,
                                    const uint8_t *seg, uint32_t len)
{
    uint16_t c = udp_checksum(src, dst, seg, len);
    if (c == 0u) {
        c = 0xFFFFu;
    }
    return c;
}

/* ── Socket lookup ────────────────────────────────────────────── */

static udp_socket_t *udp_socket_at(udpcore_t *core, int32_t sock)
{
    if (sock <= 0 || (uint32_t)sock > UDP_MAX_SOCKETS) {
        return NULL;
    }
    if (!core->socks[(uint32_t)sock - 1u].used) {
        return NULL;
    }
    return &core->socks[(uint32_t)sock - 1u];
}

int32_t udp_find_port(const udpcore_t *core, uint16_t port)
{
    uint32_t i;
    for (i = 0u; i < UDP_MAX_SOCKETS; ++i) {
        if (core->socks[i].used && core->socks[i].local_port == port) {
            return (int32_t)(i + 1u);
        }
    }
    return -1;
}

/* ── Public API ───────────────────────────────────────────────── */

udp_status_t udp_init(udpcore_t *core, uint32_t local_ip,
                      udp_ip_tx_fn ip_tx, void *ip_ctx)
{
    if (core == NULL || ip_tx == NULL) {
        return UDP_ERR_PARAM;
    }
    ow_memset(core, 0, sizeof(*core));
    core->local_ip = local_ip;
    core->ip_tx    = ip_tx;
    core->ip_ctx   = ip_ctx;
    core->initialized = true;
    return UDP_OK;
}

int32_t udp_bind(udpcore_t *core, uint32_t local_ip, uint16_t local_port)
{
    uint32_t i;

    if (core == NULL || !core->initialized || local_port == 0u) {
        return -1;
    }
    /* Reject duplicate ports. */
    if (udp_find_port(core, local_port) >= 0) {
        return -1;
    }
    for (i = 0u; i < UDP_MAX_SOCKETS; ++i) {
        if (!core->socks[i].used) {
            ow_memset(&core->socks[i], 0, sizeof(core->socks[i]));
            core->socks[i].used       = true;
            core->socks[i].local_ip   = local_ip;
            core->socks[i].local_port = local_port;
            return (int32_t)(i + 1u);
        }
    }
    return -1;
}

udp_status_t udp_close(udpcore_t *core, int32_t sock)
{
    udp_socket_t *s;

    if (core == NULL) {
        return UDP_ERR_PARAM;
    }
    s = udp_socket_at(core, sock);
    if (s == NULL) {
        return UDP_ERR_NO_PORT;
    }
    ow_memset(s, 0, sizeof(*s));
    return UDP_OK;
}

udp_status_t udp_send(udpcore_t *core, int32_t sock, uint32_t dst_ip,
                      uint16_t dst_port, const void *data, uint32_t len)
{
    udp_socket_t *s;
    udp_header_t *hdr;
    uint32_t udp_len;

    if (core == NULL || (data == NULL && len != 0u)) {
        return UDP_ERR_PARAM;
    }
    if (!core->initialized) {
        return UDP_ERR_NOT_READY;
    }
    s = udp_socket_at(core, sock);
    if (s == NULL) {
        return UDP_ERR_NO_PORT;
    }
    if (len > UDP_MAX_PAYLOAD) {
        return UDP_ERR_PARAM;
    }

    udp_len = UDP_HDR_LEN + len;
    hdr = (udp_header_t *)(void *)core->scratch;
    hdr->src_port = ow_bswap16(s->local_port);
    hdr->dst_port = ow_bswap16(dst_port);
    hdr->length   = ow_bswap16((uint16_t)udp_len);
    hdr->checksum = 0u;

    if (len != 0u) {
        ow_memcpy(core->scratch + UDP_HDR_LEN, data, len);
    }
    hdr->checksum = ow_bswap16(udp_checksum_encode(core->local_ip, dst_ip,
                                                   core->scratch, udp_len));

    return core->ip_tx(core->ip_ctx, dst_ip, UDP_PROTO, core->scratch,
                       udp_len);
}

udp_status_t udp_recv(udpcore_t *core, int32_t sock, uint32_t *out_src_ip,
                      uint16_t *out_src_port, void *buf, uint32_t cap,
                      uint32_t *out_len)
{
    udp_socket_t *s;
    uint32_t n;

    if (core == NULL || buf == NULL || out_len == NULL) {
        return UDP_ERR_PARAM;
    }
    s = udp_socket_at(core, sock);
    if (s == NULL) {
        return UDP_ERR_NO_PORT;
    }
    if (!s->has_packet) {
        return UDP_ERR_AGAIN;   /* nothing queued */
    }
    if (s->len > cap) {
        return UDP_ERR_NO_SPACE;
    }

    n = s->len;
    ow_memcpy(buf, s->buf, n);
    *out_len = n;
    if (out_src_ip != NULL) {
        *out_src_ip = s->src_ip;
    }
    if (out_src_port != NULL) {
        *out_src_port = s->src_port;
    }
    s->has_packet = false;
    s->len = 0u;
    return UDP_OK;
}

udp_status_t udp_input(udpcore_t *core, uint32_t src_ip, uint32_t dst_ip,
                       const uint8_t *datagram, uint32_t len)
{
    const udp_header_t *hdr;
    uint16_t dst_port;
    uint16_t udp_len;
    uint16_t recv_csum;
    uint32_t i;
    udp_socket_t *s = NULL;

    if (core == NULL || datagram == NULL) {
        return UDP_ERR_PARAM;
    }
    if (!core->initialized) {
        return UDP_ERR_NOT_READY;
    }
    if (len < UDP_HDR_LEN) {
        return UDP_ERR_BAD_PACKET;
    }

    hdr = (const udp_header_t *)(const void *)datagram;
    dst_port = ow_bswap16(hdr->dst_port);
    udp_len  = ow_bswap16(hdr->length);
    recv_csum = ow_bswap16(hdr->checksum);

    if (udp_len < UDP_HDR_LEN || udp_len > len) {
        return UDP_ERR_BAD_PACKET;
    }
    if (recv_csum != 0u) {
        if (udp_checksum(src_ip, dst_ip, datagram, udp_len) != 0u) {
            return UDP_ERR_BAD_PACKET;
        }
    }

    for (i = 0u; i < UDP_MAX_SOCKETS; ++i) {
        if (!core->socks[i].used) {
            continue;
        }
        if (core->socks[i].local_port != dst_port) {
            continue;
        }
        if (core->socks[i].local_ip != UDP_ANY_IP &&
            core->socks[i].local_ip != dst_ip) {
            continue;
        }
        s = &core->socks[i];
        break;
    }
    if (s == NULL) {
        return UDP_ERR_NO_PORT;
    }

    if (core->recv_cb != NULL) {
        core->recv_cb(core->recv_ctx, (int32_t)((uint32_t)(s - core->socks) + 1u),
                      src_ip, ow_bswap16(hdr->src_port),
                      datagram + UDP_HDR_LEN, udp_len - UDP_HDR_LEN);
        return UDP_OK;
    }

    if (s->has_packet) {
        return UDP_ERR_FULL;   /* mailbox occupied, drop */
    }
    if ((udp_len - UDP_HDR_LEN) > UDP_MAX_PAYLOAD) {
        return UDP_ERR_BAD_PACKET;
    }
    ow_memcpy(s->buf, datagram + UDP_HDR_LEN, udp_len - UDP_HDR_LEN);
    s->len        = udp_len - UDP_HDR_LEN;
    s->src_ip     = src_ip;
    s->src_port   = ow_bswap16(hdr->src_port);
    s->has_packet = true;
    return UDP_OK;
}

void udp_set_receive_cb(udpcore_t *core, udp_recv_cb_fn cb, void *ctx)
{
    if (core == NULL) {
        return;
    }
    core->recv_cb  = cb;
    core->recv_ctx = ctx;
}
