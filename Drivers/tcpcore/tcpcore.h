/*
 * tcpcore.h - OpenWindows TCP Transport Core (.owc)
 *
 * Connection-oriented transport with a static connection table, a
 * passive-open backlog, an in-order receive buffer, checksummed segment
 * construction and stop-and-wait retransmission bookkeeping.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef TCPCORE_H
#define TCPCORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t tcp_status_t;

#define TCP_OK            0x00000000u  /* Completed                    */
#define TCP_ERR_PARAM     0x00000001u  /* Invalid argument             */
#define TCP_ERR_NOT_READY 0x00000002u  /* Core not initialised         */
#define TCP_ERR_NO_CONN   0x00000003u  /* Unknown connection           */
#define TCP_ERR_NO_SPACE  0x00000004u  /* Segment/buffer too large     */
#define TCP_ERR_AGAIN     0x00000005u  /* Nothing available / busy     */
#define TCP_ERR_FULL      0x00000006u  /* Table or backlog full        */
#define TCP_ERR_BAD_PACKET 0x00000007u /* Malformed segment            */
#define TCP_ERR_CLOSED    0x00000008u  /* Connection not usable        */

/* ── Connection states ────────────────────────────────────────── */

typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_LISTEN,
    TCP_STATE_SYN_SENT,
    TCP_STATE_SYN_RCVD,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_CLOSING,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

/* ── Header flags and constants ───────────────────────────────── */

#define TCP_FIN        0x01u
#define TCP_SYN        0x02u
#define TCP_RST        0x04u
#define TCP_PSH        0x08u
#define TCP_ACK        0x10u
#define TCP_URG        0x20u

#define TCP_PROTO      6u
#define TCP_HDR_LEN    20u
#define TCP_MSS        1460u
#define TCP_IP_ADDR_ANY 0x00000000u

/* ── Capacity limits (static storage) ─────────────────────────── */

#define TCP_MAX_CONNS     8u
#define TCP_MAX_LISTENERS 4u
#define TCP_RXBUF_SIZE    4096u

/* ── Timers (in abstract ticks) ───────────────────────────────── */

#define TCP_INITIAL_RTO     60u
#define TCP_MAX_RTO         1000u
#define TCP_TIME_WAIT_TICKS 500u

/* ── Connection identifier (1-based; 0 is invalid) ────────────── */

typedef int32_t tcp_conn_id_t;

/* ── On-wire header ───────────────────────────────────────────── */

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_offset;   /* High nibble = header length in words   */
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} tcp_header_t;

/* ── IP transmit hook ─────────────────────────────────────────── */

typedef tcp_status_t (*tcp_ip_tx_fn)(void *ctx, uint32_t src_ip,
                                     uint32_t dst_ip, const void *segment,
                                     uint32_t len);

/* ── Tables and core state ────────────────────────────────────── */

typedef struct {
    tcp_state_t state;
    bool     used;
    bool     passive;
    bool     accepted;
    bool     tx_pending;                 /* Retransmit buffer valid   */
    uint32_t local_ip;
    uint32_t remote_ip;
    uint16_t local_port;
    uint16_t remote_port;
    uint32_t snd_una;                    /* Oldest unacknowledged seq */
    uint32_t snd_nxt;                    /* Next sequence to send     */
    uint32_t snd_wnd;
    uint32_t rcv_nxt;                    /* Next expected seq         */
    uint32_t rcv_wnd;
    uint32_t iss;                        /* Initial send sequence     */
    uint32_t irs;                        /* Initial receive sequence  */
    uint32_t rto;                        /* Current retransmit timeout */
    uint32_t rto_timer;
    uint32_t time_wait_timer;
    uint32_t tx_seq;                     /* Seq of saved segment      */
    uint32_t tx_end_seq;                 /* First seq after saved seg */
    uint32_t tx_len;                     /* Saved segment bytes       */
    uint16_t mss;
    uint16_t reserved;
    uint32_t rx_len;
    uint8_t  rx_buf[TCP_RXBUF_SIZE];     /* In-order receive buffer   */
    uint8_t  tx_buf[TCP_HDR_LEN + TCP_MSS]; /* Retransmit buffer      */
} tcp_conn_t;

typedef struct {
    bool     used;
    uint32_t local_ip;
    uint16_t local_port;
    uint16_t backlog;
    uint16_t pending;                    /* Children awaiting accept  */
} tcp_listener_t;

typedef struct {
    bool     initialized;
    uint32_t local_ip;
    uint16_t iss_seed;
    tcp_ip_tx_fn ip_tx;
    void    *ip_ctx;
    tcp_conn_t      conns[TCP_MAX_CONNS];
    tcp_listener_t  listeners[TCP_MAX_LISTENERS];
    uint8_t  scratch[TCP_HDR_LEN + TCP_MSS];
} tcpcore_t;

/* ── API ──────────────────────────────────────────────────────── */

tcp_status_t  tcp_init(tcpcore_t *core, uint32_t local_ip,
                       tcp_ip_tx_fn ip_tx, void *ip_ctx);
tcp_conn_id_t tcp_connect(tcpcore_t *core, uint32_t local_ip,
                          uint16_t local_port, uint32_t remote_ip,
                          uint16_t remote_port);
tcp_status_t  tcp_listen(tcpcore_t *core, uint32_t local_ip,
                         uint16_t local_port, uint16_t backlog);
tcp_status_t  tcp_send(tcpcore_t *core, tcp_conn_id_t conn,
                       const void *data, uint32_t len);
tcp_status_t  tcp_recv(tcpcore_t *core, tcp_conn_id_t conn, void *buf,
                       uint32_t cap, uint32_t *out_len);
tcp_status_t  tcp_close(tcpcore_t *core, tcp_conn_id_t conn);

/* ── Integration helpers ──────────────────────────────────────── */

tcp_status_t  tcp_input(tcpcore_t *core, uint32_t src_ip, uint32_t dst_ip,
                        const uint8_t *segment, uint32_t len);
tcp_status_t  tcp_tick(tcpcore_t *core, uint32_t elapsed_ticks);
tcp_status_t  tcp_accept(tcpcore_t *core, uint32_t local_ip,
                         uint16_t local_port, tcp_conn_id_t *out_conn);
tcp_state_t   tcp_conn_state(const tcpcore_t *core, tcp_conn_id_t conn);

#endif /* TCPCORE_H */
