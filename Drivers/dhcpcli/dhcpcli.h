/*
 * dhcpcli.h - OpenWindows DHCP Client (.owc)
 *
 * DHCP (RFC 2131) client state machine: DISCOVER/OFFER/REQUEST/ACK and
 * RELEASE, sending through a pluggable UDP transmit hook. The acquired
 * lease (address, mask, router, DNS, lease time) is held inline.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef DHCPCLI_H
#define DHCPCLI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t dhcp_status_t;

#define DHCP_OK            0x00000000u  /* Completed                    */
#define DHCP_ERR_PARAM     0x00000001u  /* Invalid argument             */
#define DHCP_ERR_NOT_READY 0x00000002u  /* Client not initialised       */
#define DHCP_ERR_STATE     0x00000003u  /* Invalid for current state    */
#define DHCP_ERR_BAD_PACKET 0x00000004u /* Malformed reply              */
#define DHCP_ERR_NAK       0x00000005u  /* Server refused the request   */
#define DHCP_ERR_NO_SPACE  0x00000006u  /* Message exceeds buffer       */

/* ── Client state machine ─────────────────────────────────────── */

typedef enum {
    DHCP_STATE_IDLE = 0,
    DHCP_STATE_SELECTING,
    DHCP_STATE_REQUESTING,
    DHCP_STATE_BOUND,
    DHCP_STATE_RENEWING,
    DHCP_STATE_RELEASING
} dhcp_state_t;

/* ── Protocol constants ───────────────────────────────────────── */

#define DHCP_SERVER_PORT    67u
#define DHCP_CLIENT_PORT    68u
#define DHCP_MAX_MESSAGE    576u
#define DHCP_MIN_MESSAGE    300u
#define DHCP_MAGIC_COOKIE   0x63825363u
#define DHCP_FLAG_BROADCAST 0x8000u
#define DHCP_IP_ANY         0x00000000u
#define DHCP_IP_BROADCAST   0xFFFFFFFFu

#define DHCPDISCOVER 1u
#define DHCPOFFER    2u
#define DHCPREQUEST  3u
#define DHCPDECLINE  4u
#define DHCPACK      5u
#define DHCPNAK      6u
#define DHCPRELEASE  7u

#define DHCP_OPT_PAD        0u
#define DHCP_OPT_SUBNET     1u
#define DHCP_OPT_ROUTER     3u
#define DHCP_OPT_DNS        6u
#define DHCP_OPT_REQUESTED  50u
#define DHCP_OPT_LEASE_TIME 51u
#define DHCP_OPT_MSG_TYPE   53u
#define DHCP_OPT_SERVER_ID  54u
#define DHCP_OPT_PARAM_LIST 55u
#define DHCP_OPT_END        255u

/* ── On-wire message ──────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint8_t  op;
    uint8_t  htype;
    uint8_t  hlen;
    uint8_t  hops;
    uint32_t xid;
    uint16_t secs;
    uint16_t flags;
    uint32_t ciaddr;
    uint32_t yiaddr;
    uint32_t siaddr;
    uint32_t giaddr;
    uint8_t  chaddr[16];
    uint8_t  sname[64];
    uint8_t  file[128];
    uint32_t magic;
    uint8_t  options[312];
} dhcp_message_t;

/* ── UDP transmit hook ────────────────────────────────────────── */

typedef dhcp_status_t (*dhcp_udp_tx_fn)(void *arg, uint32_t src_ip,
                                        uint16_t src_port, uint32_t dst_ip,
                                        uint16_t dst_port,
                                        const void *payload, uint32_t len);

/* ── Client instance (caller-provided, zero heap) ─────────────── */

typedef struct {
    dhcp_state_t state;
    bool     initialized;
    uint8_t  mac[6];
    uint16_t reserved;
    uint32_t xid;
    uint32_t client_ip;
    uint32_t offered_ip;
    uint32_t server_ip;
    uint32_t netmask;
    uint32_t router;
    uint32_t dns;
    uint32_t lease_seconds;
    uint32_t lease_remaining;
    uint32_t seed;
    dhcp_udp_tx_fn udp_tx;
    void    *udp_arg;
    uint8_t  scratch[DHCP_MAX_MESSAGE];
} dhcp_client_t;

/* ── API ──────────────────────────────────────────────────────── */

dhcp_status_t dhcp_init(dhcp_client_t *cli, const uint8_t mac[6],
                        dhcp_udp_tx_fn tx, void *arg);
dhcp_status_t dhcp_discover(dhcp_client_t *cli);
dhcp_status_t dhcp_request(dhcp_client_t *cli);
dhcp_status_t dhcp_release(dhcp_client_t *cli);

/* ── Integration helpers ──────────────────────────────────────── */

dhcp_status_t dhcp_input(dhcp_client_t *cli, const void *packet,
                         uint32_t len);
dhcp_status_t dhcp_tick(dhcp_client_t *cli, uint32_t elapsed_seconds);
dhcp_state_t  dhcp_state(const dhcp_client_t *cli);

#endif /* DHCPCLI_H */
