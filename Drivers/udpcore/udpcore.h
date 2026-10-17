/*
 * udpcore.h - OpenWindows UDP Transport Core (.owc)
 *
 * Connectionless datagram transport with a static port table and a
 * pluggable IPv4 transmit hook. Datagram checksums use the standard
 * IPv4 pseudo-header.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef UDPCORE_H
#define UDPCORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t udp_status_t;

#define UDP_OK            0x00000000u  /* Completed                    */
#define UDP_ERR_PARAM     0x00000001u  /* Invalid argument             */
#define UDP_ERR_NOT_READY 0x00000002u  /* Core not initialised         */
#define UDP_ERR_NO_PORT   0x00000003u  /* No socket bound to port      */
#define UDP_ERR_FULL      0x00000004u  /* Receive queue occupied       */
#define UDP_ERR_NO_SPACE  0x00000005u  /* Caller buffer too small      */
#define UDP_ERR_BAD_PACKET 0x00000006u /* Malformed datagram           */
#define UDP_ERR_AGAIN     0x00000007u  /* No datagram available        */

/* ── Protocol constants ───────────────────────────────────────── */

#define UDP_HDR_LEN     8u
#define UDP_PROTO       17u
#define UDP_ANY_IP      0x00000000u

/* ── Capacity limits (static storage) ─────────────────────────── */

#define UDP_MAX_SOCKETS 8u
#define UDP_MAX_PAYLOAD 1472u

/* ── On-wire header ───────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_header_t;

/* ── Callbacks ────────────────────────────────────────────────── */

typedef udp_status_t (*udp_ip_tx_fn)(void *ctx, uint32_t dst_ip,
                                     uint8_t proto, const void *datagram,
                                     uint32_t len);
typedef void (*udp_recv_cb_fn)(void *ctx, int32_t sock, uint32_t src_ip,
                               uint16_t src_port, const uint8_t *data,
                               uint32_t len);

/* ── Socket and core state ────────────────────────────────────── */

typedef struct {
    bool     used;
    bool     has_packet;       /* Single-slot receive mailbox          */
    uint16_t local_port;
    uint16_t reserved;
    uint32_t local_ip;
    uint32_t src_ip;           /* Sender of the queued datagram        */
    uint16_t src_port;
    uint16_t reserved2;
    uint32_t len;
    uint8_t  buf[UDP_MAX_PAYLOAD];
} udp_socket_t;

typedef struct {
    uint32_t local_ip;
    bool     initialized;
    udp_ip_tx_fn   ip_tx;
    void          *ip_ctx;
    udp_recv_cb_fn recv_cb;
    void          *recv_ctx;
    udp_socket_t socks[UDP_MAX_SOCKETS];
    uint8_t      scratch[UDP_HDR_LEN + UDP_MAX_PAYLOAD];
} udpcore_t;

/* ── API ──────────────────────────────────────────────────────── */

udp_status_t udp_init(udpcore_t *core, uint32_t local_ip,
                      udp_ip_tx_fn ip_tx, void *ip_ctx);
int32_t      udp_bind(udpcore_t *core, uint32_t local_ip,
                      uint16_t local_port);
udp_status_t udp_send(udpcore_t *core, int32_t sock, uint32_t dst_ip,
                      uint16_t dst_port, const void *data, uint32_t len);
udp_status_t udp_recv(udpcore_t *core, int32_t sock, uint32_t *out_src_ip,
                      uint16_t *out_src_port, void *buf, uint32_t cap,
                      uint32_t *out_len);
udp_status_t udp_close(udpcore_t *core, int32_t sock);

/* ── Integration helpers ──────────────────────────────────────── */

udp_status_t udp_input(udpcore_t *core, uint32_t src_ip, uint32_t dst_ip,
                       const uint8_t *datagram, uint32_t len);
void         udp_set_receive_cb(udpcore_t *core, udp_recv_cb_fn cb,
                                void *ctx);
int32_t      udp_find_port(const udpcore_t *core, uint16_t port);

#endif /* UDPCORE_H */
