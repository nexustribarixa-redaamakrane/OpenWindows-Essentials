/*
 * ipstack.h - OpenWindows Minimal IP Stack (.owc)
 *
 * IPv4 with ARP, a longest-prefix static routing table, and callback
 * dispatch for ICMP/ARP/TCP/UDP. Addresses are held in network byte
 * order as opaque 32-bit values.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef IPSTACK_H
#define IPSTACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t ipstack_status_t;

#define IP_OK             0x00000000u  /* Completed                    */
#define IP_ERR_PARAM      0x00000001u  /* Invalid argument             */
#define IP_ERR_NOT_READY  0x00000002u  /* Stack not initialised        */
#define IP_ERR_NO_ROUTE   0x00000003u  /* No route to destination      */
#define IP_ERR_PENDING    0x00000004u  /* ARP request in flight        */
#define IP_ERR_FULL       0x00000005u  /* Table full                   */
#define IP_ERR_DROP       0x00000006u  /* Packet intentionally dropped */
#define IP_ERR_BAD_PACKET 0x00000007u  /* Malformed packet             */

/* ── Protocol constants ───────────────────────────────────────── */

typedef uint32_t ipv4_addr_t;          /* Network byte order           */

#define IP_VERSION      4u
#define IP_HEADER_LEN   20u
#define IP_DEFAULT_TTL  64u
#define IP_BROADCAST    0xFFFFFFFFu

#define ETH_HDR_LEN     14u
#define ETH_TYPE_IP     0x0800u
#define ETH_TYPE_ARP    0x0806u

#define IP_PROTO_ICMP   1u
#define IP_PROTO_TCP    6u
#define IP_PROTO_UDP    17u

#define ARP_OP_REQUEST  1u
#define ARP_OP_REPLY    2u

/* ── Capacity limits (static storage) ─────────────────────────── */

#define IP_ROUTE_MAX      8u
#define IP_ARP_CACHE_MAX  16u
#define IP_FRAME_MAX      1536u

/* ── On-wire structures ───────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;
} ip_eth_header_t;

typedef struct __attribute__((packed)) {
    uint16_t htype;
    uint16_t ptype;
    uint8_t  hlen;
    uint8_t  plen;
    uint16_t oper;
    uint8_t  sha[6];
    uint32_t spa;
    uint8_t  tha[6];
    uint32_t tpa;
} ip_arp_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t  version_ihl;
    uint8_t  tos;
    uint16_t total_len;
    uint16_t id;
    uint16_t flags_frag;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src;
    uint32_t dst;
} ip_ipv4_header_t;

/* ── Callbacks ────────────────────────────────────────────────── */

typedef ipstack_status_t (*ipstack_link_tx_fn)(void *ctx,
                                               const uint8_t *frame,
                                               uint32_t len);
typedef void (*ipstack_arp_fn)(void *ctx, ipv4_addr_t ip,
                               const uint8_t mac[6]);
typedef void (*ipstack_icmp_fn)(void *ctx, ipv4_addr_t src, uint8_t type,
                                uint8_t code, const uint8_t *payload,
                                uint32_t len);
typedef void (*ipstack_tcp_fn)(void *ctx, ipv4_addr_t src, uint16_t src_port,
                               ipv4_addr_t dst, uint16_t dst_port,
                               const uint8_t *segment, uint32_t len);
typedef void (*ipstack_udp_fn)(void *ctx, ipv4_addr_t src, uint16_t src_port,
                               ipv4_addr_t dst, uint16_t dst_port,
                               const uint8_t *datagram, uint32_t len);

/* ── Tables and configuration ─────────────────────────────────── */

typedef struct {
    ipv4_addr_t ip;        /* Local address                        */
    ipv4_addr_t netmask;   /* Subnet mask                          */
    ipv4_addr_t gateway;   /* Default gateway (0 = none)           */
    uint8_t     mac[6];    /* Local hardware address               */
} ip_config_t;

typedef struct {
    ipv4_addr_t dest;      /* Network (masked)                     */
    ipv4_addr_t mask;      /* Prefix mask                          */
    ipv4_addr_t gateway;   /* Next hop (0 = on-link)               */
    uint16_t    iface;
    bool        used;
} ip_route_entry_t;

typedef struct {
    ipv4_addr_t ip;
    uint8_t     mac[6];
    uint16_t    age;
    bool        valid;
} ip_arp_entry_t;

typedef struct {
    ipv4_addr_t local_ip;
    ipv4_addr_t netmask;
    ipv4_addr_t gateway;
    uint8_t     local_mac[6];
    uint16_t    next_id;
    bool        initialized;
    ipstack_link_tx_fn link_tx;
    void       *link_ctx;
    ipstack_icmp_fn icmp_cb;
    ipstack_tcp_fn  tcp_cb;
    ipstack_udp_fn  udp_cb;
    ipstack_arp_fn  arp_cb;
    void       *dispatch_ctx;
    ip_route_entry_t routes[IP_ROUTE_MAX];
    ip_arp_entry_t   arp_cache[IP_ARP_CACHE_MAX];
    uint8_t     tx_scratch[IP_FRAME_MAX];
} ipstack_t;

/* ── API ──────────────────────────────────────────────────────── */

ipstack_status_t ip_init(ipstack_t *st, const ip_config_t *cfg,
                         ipstack_link_tx_fn link_tx, void *link_ctx);
ipstack_status_t ip_send(ipstack_t *st, ipv4_addr_t dst_ip, uint8_t proto,
                         const void *payload, uint32_t len);
ipstack_status_t ip_recv(ipstack_t *st, const void *frame, uint32_t len);
ipstack_status_t ip_route(ipstack_t *st, ipv4_addr_t dst_ip,
                          ipv4_addr_t *out_next_hop);
ipstack_status_t ip_arp_resolve(ipstack_t *st, ipv4_addr_t ip,
                                uint8_t out_mac[6]);

/* ── Configuration helpers ────────────────────────────────────── */

ipstack_status_t ip_set_address(ipstack_t *st, ipv4_addr_t ip,
                                ipv4_addr_t netmask, ipv4_addr_t gateway);
ipstack_status_t ip_add_route(ipstack_t *st, ipv4_addr_t dest,
                              ipv4_addr_t mask, ipv4_addr_t gateway);
void ip_set_callbacks(ipstack_t *st, ipstack_icmp_fn icmp,
                      ipstack_tcp_fn tcp, ipstack_udp_fn udp,
                      ipstack_arp_fn arp, void *ctx);
uint16_t ip_checksum(const void *data, uint32_t len);

#endif /* IPSTACK_H */
