/*
 * ipstack.c - OpenWindows Minimal IP Stack (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ipstack.h"

/* ── Endianness ───────────────────────────────────────────────── */

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#define IPSTACK_LITTLE_ENDIAN 1
#else
#define IPSTACK_LITTLE_ENDIAN 0
#endif

static inline uint16_t ow_bswap16(uint16_t v)
{
    return (uint16_t)((uint16_t)(v << 8) | (uint16_t)(v >> 8));
}

static inline uint32_t ow_bswap32(uint32_t v)
{
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

#if IPSTACK_LITTLE_ENDIAN
#define ow_htons(x) ow_bswap16(x)
#define ow_htonl(x) ow_bswap32(x)
#define ow_ntohs(x) ow_bswap16(x)
#define ow_ntohl(x) ow_bswap32(x)
#else
#define ow_htons(x) (x)
#define ow_htonl(x) (x)
#define ow_ntohs(x) (x)
#define ow_ntohl(x) (x)
#endif

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

/* One's-complement Internet checksum over an arbitrary byte range. */
uint16_t ip_checksum(const void *data, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint32_t sum = 0u;
    uint32_t i = 0u;

    while ((i + 1u) < len) {
        sum += (uint32_t)(((uint16_t)p[i] << 8) | (uint16_t)p[i + 1u]);
        i += 2u;
    }
    if (i < len) {
        sum += (uint32_t)((uint16_t)p[i] << 8);
    }
    while ((sum >> 16) != 0u) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

/* Number of leading one-bits in a (network order) prefix mask. */
static inline uint32_t ip_prefix_len(ipv4_addr_t mask)
{
    uint32_t count = 0u;
    uint32_t i;
    for (i = 0u; i < 32u; ++i) {
        if ((mask & (uint32_t)(1u << i)) != 0u) {
            count++;
        }
    }
    return count;
}

/* ── ARP cache ────────────────────────────────────────────────── */

static const ip_arp_entry_t *ip_arp_lookup(const ipstack_t *st,
                                           ipv4_addr_t ip)
{
    uint32_t i;
    for (i = 0u; i < IP_ARP_CACHE_MAX; ++i) {
        if (st->arp_cache[i].valid && st->arp_cache[i].ip == ip) {
            return &st->arp_cache[i];
        }
    }
    return NULL;
}

static void ip_arp_store(ipstack_t *st, ipv4_addr_t ip,
                         const uint8_t *mac)
{
    uint32_t i;
    uint32_t victim = 0u;
    uint16_t oldest = 0u;

    for (i = 0u; i < IP_ARP_CACHE_MAX; ++i) {
        if (st->arp_cache[i].valid && st->arp_cache[i].ip == ip) {
            ow_memcpy(st->arp_cache[i].mac, mac, 6u);
            st->arp_cache[i].age = 0u;
            return;
        }
    }
    for (i = 0u; i < IP_ARP_CACHE_MAX; ++i) {
        if (!st->arp_cache[i].valid) {
            st->arp_cache[i].ip = ip;
            ow_memcpy(st->arp_cache[i].mac, mac, 6u);
            st->arp_cache[i].age = 0u;
            st->arp_cache[i].valid = true;
            return;
        }
    }
    /* Cache full: evict the oldest entry. */
    for (i = 0u; i < IP_ARP_CACHE_MAX; ++i) {
        if (st->arp_cache[i].age >= oldest) {
            oldest = st->arp_cache[i].age;
            victim = i;
        }
    }
    st->arp_cache[victim].ip = ip;
    ow_memcpy(st->arp_cache[victim].mac, mac, 6u);
    st->arp_cache[victim].age = 0u;
}

/* ── Routing ──────────────────────────────────────────────────── */

static ipstack_status_t ip_route_lookup(const ipstack_t *st,
                                        ipv4_addr_t dst,
                                        ipv4_addr_t *out_next_hop)
{
    const ip_route_entry_t *best = NULL;
    uint32_t best_prefix = 0u;
    uint32_t i;

    for (i = 0u; i < IP_ROUTE_MAX; ++i) {
        const ip_route_entry_t *e = &st->routes[i];
        if (!e->used) {
            continue;
        }
        if ((dst & e->mask) != (e->dest & e->mask)) {
            continue;
        }
        /* Longest-prefix match wins. */
        if (best == NULL || ip_prefix_len(e->mask) > best_prefix) {
            best = e;
            best_prefix = ip_prefix_len(e->mask);
        }
    }

    if (best != NULL) {
        *out_next_hop = (best->gateway != 0u) ? best->gateway : dst;
        return IP_OK;
    }

    /* Fall back to the local subnet or the configured default gateway. */
    if (st->netmask != 0u &&
        (dst & st->netmask) == (st->local_ip & st->netmask)) {
        *out_next_hop = dst;
        return IP_OK;
    }
    if (st->gateway != 0u) {
        *out_next_hop = st->gateway;
        return IP_OK;
    }
    return IP_ERR_NO_ROUTE;
}

/* ── ARP transmit ─────────────────────────────────────────────── */

static ipstack_status_t ip_arp_transmit(ipstack_t *st, uint16_t oper,
                                        const uint8_t *dmac,
                                        ipv4_addr_t target_ip)
{
    uint8_t *frame = st->tx_scratch;
    ip_arp_packet_t *arp;
    uint32_t i;

    for (i = 0u; i < 6u; ++i) {
        frame[i]      = dmac[i];
        frame[6u + i] = st->local_mac[i];
    }
    frame[12] = (uint8_t)(ETH_TYPE_ARP >> 8);
    frame[13] = (uint8_t)(ETH_TYPE_ARP & 0xFFu);

    arp = (ip_arp_packet_t *)(void *)(frame + ETH_HDR_LEN);
    arp->htype = ow_htons(1u);
    arp->ptype = ow_htons(ETH_TYPE_IP);
    arp->hlen  = 6u;
    arp->plen  = 4u;
    arp->oper  = ow_htons(oper);
    for (i = 0u; i < 6u; ++i) {
        arp->sha[i] = st->local_mac[i];
        arp->tha[i] = dmac[i];
    }
    arp->spa = st->local_ip;
    arp->tpa = target_ip;

    return st->link_tx(st->link_ctx, frame,
                       (uint32_t)(ETH_HDR_LEN + sizeof(ip_arp_packet_t)));
}

static ipstack_status_t ip_arp_send_request(ipstack_t *st, ipv4_addr_t target)
{
    const uint8_t bcast[6] = { 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu, 0xFFu };
    return ip_arp_transmit(st, ARP_OP_REQUEST, bcast, target);
}

static ipstack_status_t ip_arp_send_reply(ipstack_t *st, ipv4_addr_t target,
                                          const uint8_t *target_mac)
{
    return ip_arp_transmit(st, ARP_OP_REPLY, target_mac, target);
}

/* ── Receive paths ────────────────────────────────────────────── */

static ipstack_status_t ip_handle_arp(ipstack_t *st, const uint8_t *buf,
                                      uint32_t len)
{
    const ip_arp_packet_t *arp;
    uint8_t  sha[6];
    uint16_t oper;
    ipv4_addr_t spa;
    ipv4_addr_t tpa;
    uint32_t i;

    if (len < (uint32_t)sizeof(ip_arp_packet_t)) {
        return IP_ERR_BAD_PACKET;
    }
    arp = (const ip_arp_packet_t *)(const void *)buf;
    if (ow_ntohs(arp->htype) != 1u) {
        return IP_ERR_DROP;
    }
    if (ow_ntohs(arp->ptype) != ETH_TYPE_IP) {
        return IP_ERR_DROP;
    }
    if (arp->hlen != 6u || arp->plen != 4u) {
        return IP_ERR_DROP;
    }

    /* Read packed fields into aligned locals. */
    for (i = 0u; i < 6u; ++i) {
        sha[i] = arp->sha[i];
    }
    spa = arp->spa;
    tpa = arp->tpa;
    oper = ow_ntohs(arp->oper);

    ip_arp_store(st, spa, sha);

    if (oper == ARP_OP_REQUEST) {
        if (tpa != st->local_ip) {
            return IP_ERR_DROP;   /* not for us */
        }
        return ip_arp_send_reply(st, spa, sha);
    }
    if (oper == ARP_OP_REPLY) {
        if (st->arp_cb != NULL) {
            st->arp_cb(st->dispatch_ctx, spa, sha);
        }
        return IP_OK;
    }
    return IP_ERR_DROP;
}

static ipstack_status_t ip_handle_ip(ipstack_t *st, const uint8_t *buf,
                                     uint32_t len)
{
    const ip_ipv4_header_t *ip;
    uint32_t ihl;
    uint32_t hdr_len;
    uint32_t total;
    uint32_t payload_len;
    uint16_t frag;
    ipv4_addr_t src;
    ipv4_addr_t dst;

    if (len < IP_HEADER_LEN) {
        return IP_ERR_BAD_PACKET;
    }
    ip = (const ip_ipv4_header_t *)(const void *)buf;
    if ((ip->version_ihl >> 4) != IP_VERSION) {
        return IP_ERR_DROP;
    }
    ihl = (uint32_t)(ip->version_ihl & 0x0Fu);
    if (ihl < 5u) {
        return IP_ERR_BAD_PACKET;
    }
    hdr_len = ihl * 4u;
    if (hdr_len > len) {
        return IP_ERR_BAD_PACKET;
    }
    total = (uint32_t)ow_ntohs(ip->total_len);
    if (total < hdr_len || total > len) {
        return IP_ERR_BAD_PACKET;
    }
    frag = ow_ntohs(ip->flags_frag);
    if ((frag & 0x1FFFu) != 0u) {
        return IP_ERR_DROP;   /* fragmented datagrams unsupported */
    }
    if (ip_checksum(buf, hdr_len) != 0u) {
        return IP_ERR_BAD_PACKET;
    }

    src = ip->src;
    dst = ip->dst;
    if (dst != st->local_ip && dst != IP_BROADCAST) {
        return IP_ERR_DROP;
    }

    payload_len = total - hdr_len;

    switch (ip->protocol) {
    case IP_PROTO_ICMP:
        if (payload_len < 8u) {
            return IP_ERR_BAD_PACKET;
        }
        if (st->icmp_cb != NULL) {
            st->icmp_cb(st->dispatch_ctx, src, buf[hdr_len],
                        buf[hdr_len + 1u], buf + hdr_len + 8u,
                        payload_len - 8u);
        }
        return IP_OK;
    case IP_PROTO_TCP:
        if (payload_len < 20u) {
            return IP_ERR_BAD_PACKET;
        }
        if (st->tcp_cb != NULL) {
            uint16_t sp = (uint16_t)(((uint16_t)buf[hdr_len] << 8) |
                                     (uint16_t)buf[hdr_len + 1u]);
            uint16_t dp = (uint16_t)(((uint16_t)buf[hdr_len + 2u] << 8) |
                                     (uint16_t)buf[hdr_len + 3u]);
            st->tcp_cb(st->dispatch_ctx, src, sp, dst, dp,
                       buf + hdr_len, payload_len);
        }
        return IP_OK;
    case IP_PROTO_UDP:
        if (payload_len < 8u) {
            return IP_ERR_BAD_PACKET;
        }
        if (st->udp_cb != NULL) {
            uint16_t sp = (uint16_t)(((uint16_t)buf[hdr_len] << 8) |
                                     (uint16_t)buf[hdr_len + 1u]);
            uint16_t dp = (uint16_t)(((uint16_t)buf[hdr_len + 2u] << 8) |
                                     (uint16_t)buf[hdr_len + 3u]);
            st->udp_cb(st->dispatch_ctx, src, sp, dst, dp,
                       buf + hdr_len + 8u, payload_len - 8u);
        }
        return IP_OK;
    default:
        return IP_ERR_DROP;
    }
}

/* ── Public API ───────────────────────────────────────────────── */

ipstack_status_t ip_init(ipstack_t *st, const ip_config_t *cfg,
                         ipstack_link_tx_fn link_tx, void *link_ctx)
{
    uint32_t i;

    if (st == NULL || cfg == NULL || link_tx == NULL) {
        return IP_ERR_PARAM;
    }

    ow_memset(st, 0, sizeof(*st));
    st->local_ip = cfg->ip;
    st->netmask  = cfg->netmask;
    st->gateway  = cfg->gateway;
    for (i = 0u; i < 6u; ++i) {
        st->local_mac[i] = cfg->mac[i];
    }
    st->link_tx    = link_tx;
    st->link_ctx   = link_ctx;
    st->next_id    = 1u;
    st->initialized = true;
    return IP_OK;
}

ipstack_status_t ip_set_address(ipstack_t *st, ipv4_addr_t ip,
                                ipv4_addr_t netmask, ipv4_addr_t gateway)
{
    if (st == NULL) {
        return IP_ERR_PARAM;
    }
    st->local_ip = ip;
    st->netmask  = netmask;
    st->gateway  = gateway;
    return IP_OK;
}

ipstack_status_t ip_add_route(ipstack_t *st, ipv4_addr_t dest,
                              ipv4_addr_t mask, ipv4_addr_t gateway)
{
    uint32_t i;

    if (st == NULL) {
        return IP_ERR_PARAM;
    }
    for (i = 0u; i < IP_ROUTE_MAX; ++i) {
        if (st->routes[i].used && st->routes[i].dest == (dest & mask) &&
            st->routes[i].mask == mask) {
            st->routes[i].gateway = gateway;
            return IP_OK;
        }
    }
    for (i = 0u; i < IP_ROUTE_MAX; ++i) {
        if (!st->routes[i].used) {
            st->routes[i].dest    = dest & mask;
            st->routes[i].mask    = mask;
            st->routes[i].gateway = gateway;
            st->routes[i].iface   = 0u;
            st->routes[i].used    = true;
            return IP_OK;
        }
    }
    return IP_ERR_FULL;
}

void ip_set_callbacks(ipstack_t *st, ipstack_icmp_fn icmp,
                      ipstack_tcp_fn tcp, ipstack_udp_fn udp,
                      ipstack_arp_fn arp, void *ctx)
{
    if (st == NULL) {
        return;
    }
    st->icmp_cb      = icmp;
    st->tcp_cb       = tcp;
    st->udp_cb       = udp;
    st->arp_cb       = arp;
    st->dispatch_ctx = ctx;
}

ipstack_status_t ip_route(ipstack_t *st, ipv4_addr_t dst_ip,
                          ipv4_addr_t *out_next_hop)
{
    if (st == NULL || out_next_hop == NULL) {
        return IP_ERR_PARAM;
    }
    if (!st->initialized) {
        return IP_ERR_NOT_READY;
    }
    return ip_route_lookup(st, dst_ip, out_next_hop);
}

ipstack_status_t ip_arp_resolve(ipstack_t *st, ipv4_addr_t ip,
                                uint8_t out_mac[6])
{
    const ip_arp_entry_t *entry;
    uint32_t i;

    if (st == NULL || out_mac == NULL) {
        return IP_ERR_PARAM;
    }
    if (!st->initialized) {
        return IP_ERR_NOT_READY;
    }

    if (ip == st->local_ip) {
        for (i = 0u; i < 6u; ++i) {
            out_mac[i] = st->local_mac[i];
        }
        return IP_OK;
    }

    entry = ip_arp_lookup(st, ip);
    if (entry != NULL) {
        for (i = 0u; i < 6u; ++i) {
            out_mac[i] = entry->mac[i];
        }
        return IP_OK;
    }

    if (ip_arp_send_request(st, ip) != IP_OK) {
        return IP_ERR_NO_ROUTE;
    }
    return IP_ERR_PENDING;
}

ipstack_status_t ip_send(ipstack_t *st, ipv4_addr_t dst_ip, uint8_t proto,
                         const void *payload, uint32_t len)
{
    uint8_t *frame;
    ip_ipv4_header_t *ip;
    ipv4_addr_t next_hop;
    uint8_t next_mac[6];
    ipstack_status_t rc;
    uint32_t total;
    uint32_t i;

    if (st == NULL || (payload == NULL && len != 0u)) {
        return IP_ERR_PARAM;
    }
    if (!st->initialized) {
        return IP_ERR_NOT_READY;
    }
    if (len > (uint32_t)(IP_FRAME_MAX - ETH_HDR_LEN - IP_HEADER_LEN)) {
        return IP_ERR_PARAM;
    }

    if (dst_ip == IP_BROADCAST) {
        for (i = 0u; i < 6u; ++i) {
            next_mac[i] = 0xFFu;
        }
    } else {
        rc = ip_route_lookup(st, dst_ip, &next_hop);
        if (rc != IP_OK) {
            return rc;
        }
        rc = ip_arp_resolve(st, next_hop, next_mac);
        if (rc == IP_ERR_PENDING) {
            return IP_ERR_PENDING;   /* ARP request sent; retry later */
        }
        if (rc != IP_OK) {
            return IP_ERR_NO_ROUTE;
        }
    }

    frame = st->tx_scratch;
    for (i = 0u; i < 6u; ++i) {
        frame[i]      = next_mac[i];
        frame[6u + i] = st->local_mac[i];
    }
    frame[12] = (uint8_t)(ETH_TYPE_IP >> 8);
    frame[13] = (uint8_t)(ETH_TYPE_IP & 0xFFu);

    ip = (ip_ipv4_header_t *)(void *)(frame + ETH_HDR_LEN);
    ip->version_ihl = 0x45u;
    ip->tos         = 0u;
    total           = (uint32_t)IP_HEADER_LEN + len;
    ip->total_len   = ow_htons((uint16_t)total);
    ip->id          = ow_htons(st->next_id);
    st->next_id     = (uint16_t)(st->next_id + 1u);
    ip->flags_frag  = 0u;
    ip->ttl         = IP_DEFAULT_TTL;
    ip->protocol    = proto;
    ip->checksum    = 0u;
    ip->src         = st->local_ip;
    ip->dst         = dst_ip;

    if (len != 0u) {
        ow_memcpy(frame + ETH_HDR_LEN + IP_HEADER_LEN, payload, len);
    }
    ip->checksum = ow_htons(ip_checksum(ip, IP_HEADER_LEN));

    return st->link_tx(st->link_ctx, frame, ETH_HDR_LEN + total);
}

ipstack_status_t ip_recv(ipstack_t *st, const void *frame, uint32_t len)
{
    const uint8_t *f = (const uint8_t *)frame;
    uint16_t ethertype;

    if (st == NULL || frame == NULL) {
        return IP_ERR_PARAM;
    }
    if (!st->initialized) {
        return IP_ERR_NOT_READY;
    }
    if (len < ETH_HDR_LEN) {
        return IP_ERR_BAD_PACKET;
    }

    ethertype = (uint16_t)(((uint16_t)f[12] << 8) | (uint16_t)f[13]);
    if (ethertype == ETH_TYPE_ARP) {
        return ip_handle_arp(st, f + ETH_HDR_LEN, len - ETH_HDR_LEN);
    }
    if (ethertype == ETH_TYPE_IP) {
        return ip_handle_ip(st, f + ETH_HDR_LEN, len - ETH_HDR_LEN);
    }
    return IP_ERR_DROP;
}
