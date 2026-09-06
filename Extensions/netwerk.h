/*
 * netwerk.h - netwerk.owd ABI mirror (In-Pipe Session / Crypto-Link Layer)
 *
 * Managed session and crypto-link layer for OpenWindows modules. Sits on
 * top of owcrypt.owd and provides the transport-adjacent policy a network
 * stack needs without owning a socket abstraction:
 *
 *   - Endpoint/link registry (fixed-size, zero allocation, handles reused)
 *   - Keep-alive heartbeat state machine with dead-link detection
 *   - SUTF-sealed link frames guarded by a 64-slot replay window and a
 *     cheap envelope-based pre-decode replay drop (no crypto work for
 *     replayed or out-of-window frames)
 *   - Ingress BANcode stream scan wired to the caller's ban signal hook
 *     (the banhammer damage-control plane on link eve)
 *
 * Wire layout of a link frame (all counts big-endian, lengths checked
 * before touching the sealed region):
 *
 *     [ NWL1 ][ ver 1 ][ seq 8 BE ][ plen 4 BE ]
 *     [ ---- owcrypt SUTF frame (aad = seq 8 BE) ---- ]
 *
 * The envelope is unauthenticated by design: it exists only so a replayed
 * or malformed frame can be dropped before any cryptographic work. The
 * sequence number is re-authenticated inside the sealed region, so a
 * forged envelope can never advance the replay window.
 *
 * Conforms to OWD1 binary format (Extensions/owd_format.h).
 * Depends on owcrypt.owd (Extensions/owcrypt.h) and kernel64.owd.
 * C99 freestanding - <stdint.h>/<stdbool.h>/<stddef.h>/<limits.h> only.
 */

#ifndef OWE_NETWERK_H
#define OWE_NETWERK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  OWD1 binary header constants                                       */
/* ------------------------------------------------------------------ */

#define NETW_LIB_TYPE        OWD_LIBTYPE_HYBRID   /* 0x02 user+kernel  */
#define NETW_TARGET_ARCH     0x02u                /* x86_64            */
#define NETW_ALIGNMENT_LOG2  4u

#define NETW_LIB_NAME        "netwerk.owd"
#define NETW_LIB_VERSION     "netwerk.owd 1.0.0"

/* ------------------------------------------------------------------ */
/*  Status codes - BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A7FF): Fatal module faults                    */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal degradations                 */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                     */
/* ------------------------------------------------------------------ */

typedef uint32_t netwerk_status_t;

#define NETW_OK                          0x00000000u  /* Success        */
/* B+ Fatal */
#define NETW_BAN_K64_BOOT                0x0011A340u  /* k64 API dead   */
#define NETW_BAN_AUTH_FAIL               0x0011A341u  /* tag mismatch   */
#define NETW_BAN_REPLAY                  0x0011A342u  /* replay / OOW   */
#define NETW_BAN_BANCODE                 0x0011A343u  /* payload BAN    */
#define NETW_BAN_ENVELOPE                0x0011A344u  /* frame envelope */
#define NETW_BAN_LINK_DEAD               0x0011A345u  /* keep-alive     */
/* W+ Warning */
#define NETW_WARN_BEATS_MISSED           0x0011A840u  /* slow/dropped   */
/* S+ Soft */
#define NETW_ERR_UNINITIALIZED           0x0011AEC8u  /* Not init yet   */
#define NETW_ERR_BAD_ARG                 0x0011AEC9u  /* Bad argument   */
#define NETW_ERR_SHORT_OUT               0x0011AECAu  /* Output too small*/
#define NETW_ERR_STATE                   0x0011AECBu  /* Wrong link state*/
#define NETW_ERR_LINKS_FULL              0x0011AECCu  /* Registry full  */

/* ------------------------------------------------------------------ */
/*  Registry sizing                                                    */
/* ------------------------------------------------------------------ */

#define NETW_MAX_LINKS          16u     /* Fixed registry capacity      */

/* ------------------------------------------------------------------ */
/*  Link direction                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    NETW_DIR_INBOUND  = 0u,             /* Link established by remote  */
    NETW_DIR_OUTBOUND = 1u              /* Link established by us      */
} netwerk_dir_t;

/* ------------------------------------------------------------------ */
/*  Link state machine                                                 */
/*  DOWN -> SECURE (activate) -> DEAD | BANNED; close returns to DOWN  */
/* ------------------------------------------------------------------ */

typedef enum {
    NETW_LINK_STATE_DOWN    = 0u,       /* Slot free / closed          */
    NETW_LINK_STATE_SYN     = 1u,       /* Handshake in progress       */
    NETW_LINK_STATE_SECURE  = 2u,       /* Established, alive          */
    NETW_LINK_STATE_DEAD    = 3u,       /* Keep-alive timeout          */
    NETW_LINK_STATE_BANNED  = 4u        /* Ingress BANcode fired       */
} netwerk_link_state_t;

/* ------------------------------------------------------------------ */
/*  Replay window                                                      */
/* ------------------------------------------------------------------ */

#define NETW_REPLAY_WINDOW   64u        /* Slots below rx_hi accepted  */

/* ------------------------------------------------------------------ */
/*  Link frame envelope                                                */
/*  [ magic 4 ][ ver 1 ][ seq 8 BE ][ plen 4 BE ]                      */
/* ------------------------------------------------------------------ */

#define NETW_FRAME_MAGIC     0x4E574C31u       /* "NWL1" BE32 read     */
#define NETW_FRAME_VERSION   0x01u
#define NETW_LINK_ENVELOPE   17u               /* Unsealed header size */
#define NETW_SEQ_AAD         8u                /* seq fed as SUTF AAD  */

/* ------------------------------------------------------------------ */
/*  Per-link record                                                    */
/* ------------------------------------------------------------------ */

typedef void (*netwerk_ban_signal_t)(uint32_t bancode, uint64_t subcode,
                                     void *ctx);

typedef struct netwerk_link {
    netwerk_link_state_t state;         /* Current lifecycle state     */
    netwerk_dir_t        dir;           /* Inbound / outbound          */
    uint16_t             id;            /* 1-based handle, 0 = free    */
    uint8_t              peer[16];      /* Opaque peer identifier      */
    struct netwerk_session *owner;      /* Owning registry (may null)  */

    uint64_t             tx_seq;        /* Next outbound sequence      */
    uint64_t             rx_hi;         /* Highest accepted inbound seq*/
    uint64_t             rx_mask;       /* Recent-accept bitmap        */

    uint32_t             keepalive_ms;  /* Heartbeat period (0=never)  */
    uint32_t             timeout_ms;    /* Dead after silence (0=never)*/
    uint64_t             last_activity; /* Last rx/tx tick             */
    uint64_t             interval_count;/* Completed keepalive periods */
    uint32_t             missed_beats;  /* Periods elapsed, no traffic */
    uint32_t             heartbeats;    /* Times keepalive was due     */

    uint64_t             frames_tx;     /* Statistics                  */
    uint64_t             frames_rx;
    uint64_t             bytes_tx;
    uint64_t             bytes_rx;
    uint32_t             auth_fails;
    uint32_t             replays;
    uint32_t             bans;

    netwerk_ban_signal_t ban_hook;      /* Caller ban signal (may null)*/
    void                *ban_ctx;       /* Caller ban context          */
    uint32_t             reserved[4];
} netwerk_link_t;

/* ------------------------------------------------------------------ */
/*  Session registry                                                   */
/* ------------------------------------------------------------------ */

typedef struct netwerk_session {
    netwerk_link_t       links[NETW_MAX_LINKS];
    netwerk_ban_signal_t ban_hook;      /* Default for new links       */
    void                *ban_ctx;
    uint64_t             total_frames_tx;
    uint64_t             total_frames_rx;
    uint32_t             total_auth_fails;
    uint32_t             total_bans;
    uint32_t             link_count;
    bool                 initialized;
    uint8_t              reserved[7];
} netwerk_session_t;

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

netwerk_status_t netwerk_module_init(void);
netwerk_status_t netwerk_module_shutdown(void);
uint32_t         netwerk_abi_version(void);
uint16_t         netwerk_abi_major(void);
uint16_t         netwerk_abi_minor(void);
const uint8_t   *netwerk_ident(void);

/* ------------------------------------------------------------------ */
/*  Session registry                                                   */
/* ------------------------------------------------------------------ */

void          netwerk_session_init(netwerk_session_t *sess);
void          netwerk_session_reset(netwerk_session_t *sess);
netwerk_status_t netwerk_session_set_ban_signal(netwerk_session_t *sess,
                                                netwerk_ban_signal_t hook,
                                                void *ctx);
netwerk_status_t netwerk_link_alloc(netwerk_session_t *sess,
                                    netwerk_dir_t dir,
                                    const uint8_t peer[16],
                                    netwerk_link_t **out);
netwerk_status_t netwerk_link_free(netwerk_session_t *sess,
                                   netwerk_link_t *ln);
const netwerk_link_t *netwerk_session_find(const netwerk_session_t *sess,
                                           uint16_t id);
uint32_t netwerk_session_link_count(const netwerk_session_t *sess);
void     netwerk_session_stats(const netwerk_session_t *sess,
                               uint64_t *frames_tx, uint64_t *frames_rx,
                               uint32_t *auth_fails, uint32_t *bans);

/* ------------------------------------------------------------------ */
/*  Link state                                                         */
/* ------------------------------------------------------------------ */

netwerk_link_state_t netwerk_link_state(const netwerk_link_t *ln);
netwerk_status_t netwerk_link_reset(netwerk_link_t *ln);
netwerk_status_t netwerk_link_activate(netwerk_link_t *ln);
netwerk_status_t netwerk_link_close(netwerk_link_t *ln);
netwerk_status_t netwerk_link_set_keepalive(netwerk_link_t *ln,
                                            uint32_t keepalive_ms,
                                            uint32_t timeout_ms);
netwerk_status_t netwerk_link_set_ban_signal(netwerk_link_t *ln,
                                             netwerk_ban_signal_t hook,
                                             void *ctx);

/* ------------------------------------------------------------------ */
/*  Keep-alive state machine                                           */
/*  `now` is a monotonic uptime tick; pass 0 to read it from k64.      */
/* ------------------------------------------------------------------ */

netwerk_status_t netwerk_link_tx(netwerk_link_t *ln, uint64_t now);
netwerk_status_t netwerk_link_rx(netwerk_link_t *ln, uint64_t now);
netwerk_status_t netwerk_link_tick(netwerk_link_t *ln, uint64_t now);
bool             netwerk_link_is_alive(const netwerk_link_t *ln,
                                       uint64_t now);
uint32_t         netwerk_link_next_beat_delta_ms(const netwerk_link_t *ln,
                                                 uint64_t now);

/* ------------------------------------------------------------------ */
/*  Crypto-link framing (owcrypt.owd backed)                           */
/*  key[32] + per-seq nonce; nonce = 4 zero bytes + seq (8 bytes LE).  */
/* ------------------------------------------------------------------ */

void netwerk_link_make_nonce(uint64_t seq, uint8_t nonce[12]);

/* Seal `plain` into a link frame. Requires a caller buffer of at least
 * NETW_LINK_ENVELOPE + OWCRYPT_FRAME_OVERHEAD + 8 + plen + 16.
 * Bumps the link's outbound stats. */
netwerk_status_t netwerk_seal_link_frame(netwerk_link_t *ln,
                                         const uint8_t key[32],
                                         uint64_t seq,
                                         const uint8_t *plain, size_t plen,
                                         uint8_t *frame, size_t *frame_len);

/* Open a link frame: envelope check, replay pre-check, then a full
 * authenticated open of the sealed region. On success the caller gets
 * the plaintext and the recovered AAD (the 8-byte BE sequence number,
 * pointing into the frame buffer). Replayed frames are dropped with
 * NETW_BAN_REPLAY before any crypto work. */
netwerk_status_t netwerk_open_link_frame(netwerk_link_t *ln,
                                         const uint8_t key[32],
                                         const uint8_t *frame,
                                         size_t frame_len,
                                         uint8_t *plain_out,
                                         size_t *plain_len,
                                         const uint8_t **aad_out,
                                         size_t *aad_len_out,
                                         bool scan_ban);

#ifdef __cplusplus
}
#endif

#endif /* OWE_NETWERK_H */