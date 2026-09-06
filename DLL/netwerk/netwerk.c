/*
 * netwerk.c - netwerk.owd (In-Pipe Session / Crypto-Link Layer)
 *
 * Managed session and crypto-link policy layer over owcrypt.owd:
 * a fixed-size endpoint/link registry, a keep-alive heartbeat state
 * machine, SUTF-sealed link frames with a 64-slot replay window, and
 * ingress BANcode pre-scan wired to the caller's ban signal hook.
 *
 * Wire layout (all counts big-endian):
 *     [ "NWL1" ][ ver 1 ][ seq 8 BE ][ plen 4 BE ]
 *     [ --- owcrypt SUTF frame, aad = seq (8 bytes BE) --- ]
 *
 * The envelope is deliberately unauthenticated: replay / malformed
 * frames are dropped before any cryptographic work. The sequence number
 * is re-authenticated inside the sealed region, so a forged envelope
 * can never advance the replay window.
 *
 * Conforms to OWD1 binary format (Extensions/owd_format.h).
 * C99 freestanding - no heap, no hosted libc.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#include "netwerk.h"            /* Extensions ABI mirror */
#include "owcrypt.h"
#include "owd_format.h"
#include "owc_format.h"
#include "kernel64.h"

/* OWD1 binary header metadata (documentary; see Extensions/owd_format.h). */
#define NETW_VERSION_STRING     NETW_LIB_VERSION

#define NETW_INIT_FLAGS         (OWC_INIT_REQUIRES_OWRP | \
                                 OWC_INIT_REQUIRES_BANC)

static const uint8_t netwerk_ident_str[] = NETW_VERSION_STRING;

static bool netwerk_initialized = false;

/* ------------------------------------------------------------------ */
/*  Big-endian helpers                                                 */
/* ------------------------------------------------------------------ */

static uint32_t owc_get32be(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static void owc_put32be(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)(v);
}

static uint64_t owc_get64be(const uint8_t *p)
{
    return ((uint64_t)owc_get32be(p) << 32) | owc_get32be(p + 4);
}

static void owc_put64be(uint8_t *p, uint64_t v)
{
    owc_put32be(p, (uint32_t)(v >> 32));
    owc_put32be(p + 4, (uint32_t)v);
}

/* ------------------------------------------------------------------ */
/*  Lifecycle                                                          */
/* ------------------------------------------------------------------ */

static netwerk_status_t netw_now(uint64_t now_in, uint64_t *out)
{
    if (now_in != 0u) {
        *out = now_in;
        return NETW_OK;
    }
    if (k64_query_system_time(out) != K64_OK) {
        return NETW_BAN_K64_BOOT;
    }
    return NETW_OK;
}

netwerk_status_t netwerk_module_init(void)
{
    if (netwerk_initialized) {
        return NETW_OK;
    }
    if (k64_initialize_api() != K64_OK) {
        return NETW_BAN_K64_BOOT;
    }
    if (owcrypt_module_init() != OWCRYPT_OK) {
        return NETW_BAN_K64_BOOT;
    }
    netwerk_initialized = true;
    return NETW_OK;
}

netwerk_status_t netwerk_module_shutdown(void)
{
    if (!netwerk_initialized) {
        return NETW_ERR_UNINITIALIZED;
    }
    netwerk_initialized = false;
    return NETW_OK;
}

uint32_t netwerk_abi_version(void)
{
    return 0x00010000u;     /* 1.0.0 encoded (major<<16 | minor<<8)  */
}

uint16_t netwerk_abi_major(void)
{
    return 1u;
}

uint16_t netwerk_abi_minor(void)
{
    return 0u;
}

const uint8_t *netwerk_ident(void)
{
    return netwerk_ident_str;
}

/* ------------------------------------------------------------------ */
/*  Session registry                                                   */
/* ------------------------------------------------------------------ */

void netwerk_session_init(netwerk_session_t *sess)
{
    if (sess == NULL) {
        return;
    }
    owcrypt_cleanse(sess, sizeof(*sess));
    sess->initialized = true;
}

void netwerk_session_reset(netwerk_session_t *sess)
{
    if (sess == NULL) {
        return;
    }
    owcrypt_cleanse(sess, sizeof(*sess));
}

netwerk_status_t netwerk_session_set_ban_signal(netwerk_session_t *sess,
                                                netwerk_ban_signal_t hook,
                                                void *ctx)
{
    if (sess == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    sess->ban_hook = hook;
    sess->ban_ctx = ctx;
    return NETW_OK;
}

netwerk_status_t netwerk_link_alloc(netwerk_session_t *sess,
                                    netwerk_dir_t dir,
                                    const uint8_t peer[16],
                                    netwerk_link_t **out)
{
    size_t i;
    netwerk_link_t *ln;
    uint32_t j;
    if (sess == NULL || out == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    if (!sess->initialized) {
        return NETW_ERR_UNINITIALIZED;
    }
    if (dir != NETW_DIR_INBOUND && dir != NETW_DIR_OUTBOUND) {
        return NETW_ERR_BAD_ARG;
    }
    if (sess->link_count >= NETW_MAX_LINKS) {
        return NETW_ERR_LINKS_FULL;
    }
    ln = NULL;
    for (i = 0u; i < NETW_MAX_LINKS; i++) {
        if (sess->links[i].id == 0u) {
            ln = &sess->links[i];
            break;
        }
    }
    if (ln == NULL) {
        return NETW_ERR_LINKS_FULL;
    }
    owcrypt_cleanse(ln, sizeof(*ln));
    ln->id = (uint16_t)(i + 1u);
    ln->state = NETW_LINK_STATE_SECURE;
    ln->dir = dir;
    ln->owner = sess;
    if (peer != NULL) {
        for (j = 0u; j < 16u; j++) {
            ln->peer[j] = peer[j];
        }
    }
    ln->ban_hook = sess->ban_hook;
    ln->ban_ctx = sess->ban_ctx;
    sess->link_count++;
    *out = ln;
    return NETW_OK;
}

netwerk_status_t netwerk_link_free(netwerk_session_t *sess,
                                   netwerk_link_t *ln)
{
    if (sess == NULL || ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    if (ln->id == 0u) {
        return NETW_ERR_BAD_ARG;
    }
    if (sess->link_count > 0u) {
        sess->link_count--;
    }
    ln->state = NETW_LINK_STATE_DOWN;
    ln->id = 0u;
    ln->owner = NULL;
    return NETW_OK;
}

const netwerk_link_t *netwerk_session_find(const netwerk_session_t *sess,
                                           uint16_t id)
{
    size_t i;
    if (sess == NULL || id == 0u) {
        return NULL;
    }
    for (i = 0u; i < NETW_MAX_LINKS; i++) {
        if (sess->links[i].id == id) {
            return &sess->links[i];
        }
    }
    return NULL;
}

uint32_t netwerk_session_link_count(const netwerk_session_t *sess)
{
    return (sess == NULL) ? 0u : sess->link_count;
}

void netwerk_session_stats(const netwerk_session_t *sess,
                           uint64_t *frames_tx, uint64_t *frames_rx,
                           uint32_t *auth_fails, uint32_t *bans)
{
    if (sess == NULL) {
        return;
    }
    if (frames_tx != NULL) { *frames_tx = sess->total_frames_tx; }
    if (frames_rx != NULL) { *frames_rx = sess->total_frames_rx; }
    if (auth_fails != NULL) { *auth_fails = sess->total_auth_fails; }
    if (bans != NULL) { *bans = sess->total_bans; }
}

/* ------------------------------------------------------------------ */
/*  Link state                                                         */
/* ------------------------------------------------------------------ */

netwerk_link_state_t netwerk_link_state(const netwerk_link_t *ln)
{
    return (ln == NULL) ? NETW_LINK_STATE_DOWN : ln->state;
}

netwerk_status_t netwerk_link_reset(netwerk_link_t *ln)
{
    if (ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    owcrypt_cleanse(ln, sizeof(*ln));
    return NETW_OK;
}

netwerk_status_t netwerk_link_activate(netwerk_link_t *ln)
{
    if (ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    if (ln->state == NETW_LINK_STATE_BANNED) {
        return NETW_ERR_STATE;
    }
    if (ln->state == NETW_LINK_STATE_SECURE) {
        return NETW_OK;
    }
    ln->state = NETW_LINK_STATE_SECURE;
    ln->missed_beats = 0u;
    ln->interval_count = 0u;
    return NETW_OK;
}

netwerk_status_t netwerk_link_close(netwerk_link_t *ln)
{
    if (ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    ln->state = NETW_LINK_STATE_DOWN;
    ln->missed_beats = 0u;
    ln->interval_count = 0u;
    return NETW_OK;
}

netwerk_status_t netwerk_link_set_keepalive(netwerk_link_t *ln,
                                            uint32_t keepalive_ms,
                                            uint32_t timeout_ms)
{
    if (ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    if (keepalive_ms != 0u && timeout_ms != 0u &&
        timeout_ms < keepalive_ms) {
        return NETW_ERR_BAD_ARG;
    }
    if ((keepalive_ms != 0u) != (timeout_ms != 0u)) {
        return NETW_ERR_BAD_ARG;
    }
    ln->keepalive_ms = keepalive_ms;
    ln->timeout_ms = timeout_ms;
    ln->missed_beats = 0u;
    ln->interval_count = 0u;
    return NETW_OK;
}

netwerk_status_t netwerk_link_set_ban_signal(netwerk_link_t *ln,
                                             netwerk_ban_signal_t hook,
                                             void *ctx)
{
    if (ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    ln->ban_hook = hook;
    ln->ban_ctx = ctx;
    return NETW_OK;
}

/* ------------------------------------------------------------------ */
/*  Keep-alive state machine                                           */
/* ------------------------------------------------------------------ */

static void netw_note_activity(netwerk_link_t *ln, uint64_t now)
{
    ln->last_activity = now;
    ln->interval_count = 0u;
    ln->missed_beats = 0u;
}

netwerk_status_t netwerk_link_tx(netwerk_link_t *ln, uint64_t now)
{
    uint64_t t;
    netwerk_status_t st;
    if (ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    st = netw_now(now, &t);
    if (st != NETW_OK) {
        return st;
    }
    if (ln->state == NETW_LINK_STATE_BANNED) {
        return NETW_ERR_STATE;
    }
    netw_note_activity(ln, t);
    return NETW_OK;
}

netwerk_status_t netwerk_link_rx(netwerk_link_t *ln, uint64_t now)
{
    uint64_t t;
    netwerk_status_t st;
    if (ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    st = netw_now(now, &t);
    if (st != NETW_OK) {
        return st;
    }
    if (ln->state == NETW_LINK_STATE_BANNED) {
        return NETW_ERR_STATE;
    }
    netw_note_activity(ln, t);
    return NETW_OK;
}

netwerk_status_t netwerk_link_tick(netwerk_link_t *ln, uint64_t now)
{
    uint64_t t, elapsed, full, missed;
    netwerk_status_t st;
    if (ln == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    st = netw_now(now, &t);
    if (st != NETW_OK) {
        return st;
    }
    if (ln->state == NETW_LINK_STATE_BANNED) {
        return NETW_ERR_STATE;
    }
    if (ln->state == NETW_LINK_STATE_DEAD) {
        return NETW_BAN_LINK_DEAD;
    }
    if (ln->state != NETW_LINK_STATE_SECURE) {
        return NETW_ERR_STATE;
    }
    if (ln->keepalive_ms == 0u) {
        return NETW_OK;
    }
    if (t < ln->last_activity) {
        /* Monotonic clock reset: restart the accounting window. */
        ln->last_activity = t;
        ln->interval_count = 0u;
        return NETW_OK;
    }
    elapsed = t - ln->last_activity;
    full = elapsed / ln->keepalive_ms;
    if (full > ln->interval_count) {
        missed = full - ln->interval_count;
        ln->interval_count = full;
        ln->missed_beats += (uint32_t)(missed > UINT32_MAX
                                       ? UINT32_MAX : missed);
        ln->heartbeats += (uint32_t)(missed > UINT32_MAX
                                     ? UINT32_MAX : missed);
    }
    if (ln->timeout_ms != 0u && elapsed >= ln->timeout_ms) {
        ln->state = NETW_LINK_STATE_DEAD;
        return NETW_BAN_LINK_DEAD;
    }
    return ln->missed_beats > 0u ? NETW_WARN_BEATS_MISSED : NETW_OK;
}

bool netwerk_link_is_alive(const netwerk_link_t *ln, uint64_t now)
{
    uint64_t elapsed;
    if (ln == NULL) {
        return false;
    }
    if (ln->state == NETW_LINK_STATE_DEAD ||
        ln->state == NETW_LINK_STATE_BANNED) {
        return false;
    }
    if (ln->timeout_ms == 0u) {
        return ln->state == NETW_LINK_STATE_SECURE;
    }
    elapsed = (now > ln->last_activity) ? (now - ln->last_activity) : 0u;
    return elapsed < ln->timeout_ms;
}

uint32_t netwerk_link_next_beat_delta_ms(const netwerk_link_t *ln,
                                         uint64_t now)
{
    uint64_t elapsed, mod;
    if (ln == NULL || ln->keepalive_ms == 0u) {
        return UINT32_MAX;
    }
    elapsed = (now > ln->last_activity) ? (now - ln->last_activity) : 0u;
    mod = elapsed % ln->keepalive_ms;
    return (mod == 0u) ? 0u : (uint32_t)(ln->keepalive_ms - mod);
}

/* ------------------------------------------------------------------ */
/*  Crypto-link framing (owcrypt.owd backed)                           */
/* ------------------------------------------------------------------ */

void netwerk_link_make_nonce(uint64_t seq, uint8_t nonce[12])
{
    nonce[0] = 0u;
    nonce[1] = 0u;
    nonce[2] = 0u;
    nonce[3] = 0u;
    nonce[4]    = (uint8_t)(seq);
    nonce[5]    = (uint8_t)(seq >> 8);
    nonce[6]    = (uint8_t)(seq >> 16);
    nonce[7]    = (uint8_t)(seq >> 24);
    nonce[8]    = (uint8_t)(seq >> 32);
    nonce[9]    = (uint8_t)(seq >> 40);
    nonce[10]   = (uint8_t)(seq >> 48);
    nonce[11]   = (uint8_t)(seq >> 56);
}

static void netw_seq_aad(uint64_t seq, uint8_t aad[8])
{
    owc_put64be(aad, seq);
}

/* Ban relay: forward an owcrypt-detected BANcode to the link's hook. */
static void netw_ban_relay(uint32_t bancode, uint64_t subcode, void *ctx)
{
    netwerk_link_t *ln = (netwerk_link_t *)ctx;
    if (ln != NULL && ln->ban_hook != NULL) {
        ln->ban_hook(bancode, subcode, ln->ban_ctx);
    }
}

/* Replay window: reject seq strictly older than 64 slots below rx_hi,
 * or already accepted. Zero crypto work on this path. */
static bool netw_replay_ok(const netwerk_link_t *ln, uint64_t seq)
{
    if (seq > ln->rx_hi) {
        return true;
    }
    if (ln->rx_hi - seq >= NETW_REPLAY_WINDOW) {
        return false;
    }
    return (ln->rx_mask & (1ull << (ln->rx_hi - seq))) == 0u;
}

static void netw_replay_accept(netwerk_link_t *ln, uint64_t seq)
{
    if (seq > ln->rx_hi) {
        if (seq - ln->rx_hi >= NETW_REPLAY_WINDOW) {
            ln->rx_mask = 1u;
        } else {
            ln->rx_mask <<= (unsigned)(seq - ln->rx_hi);
            ln->rx_mask |= 1u;
        }
        ln->rx_hi = seq;
    } else {
        ln->rx_mask |= (1ull << (ln->rx_hi - seq));
    }
}

netwerk_status_t netwerk_seal_link_frame(netwerk_link_t *ln,
                                         const uint8_t key[32],
                                         uint64_t seq,
                                         const uint8_t *plain, size_t plen,
                                         uint8_t *frame, size_t *frame_len)
{
    uint8_t   aad[NETW_SEQ_AAD];
    uint8_t   nonce[12];
    uint8_t  *sutf;
    size_t    sutf_len;
    size_t    need;
    uint8_t   *e = frame;                 /* Envelope in place           */
    owcrypt_status_t st;

    if (ln == NULL || key == NULL || frame == NULL || frame_len == NULL ||
        (plen > 0u && plain == NULL)) {
        return NETW_ERR_BAD_ARG;
    }
    if (ln->id == 0u) {
        return NETW_ERR_BAD_ARG;
    }
    if (ln->state == NETW_LINK_STATE_BANNED) {
        return NETW_ERR_STATE;
    }
    if (ln->state == NETW_LINK_STATE_DEAD) {
        return NETW_BAN_LINK_DEAD;
    }
    need = NETW_LINK_ENVELOPE + OWCRYPT_FRAME_OVERHEAD + NETW_SEQ_AAD +
           plen + 16u;
    if (*frame_len < need) {
        return NETW_ERR_SHORT_OUT;
    }

    netw_seq_aad(seq, aad);
    netwerk_link_make_nonce(seq, nonce);

    sutf = frame + NETW_LINK_ENVELOPE;
    sutf_len = *frame_len - NETW_LINK_ENVELOPE;
    st = owcrypt_seal_frame(key, nonce, aad, NETW_SEQ_AAD,
                            plain, plen, sutf, &sutf_len);
    if (st != OWCRYPT_OK) {
        if (st == OWCRYPT_ERR_BAD_ARG) {
            return NETW_ERR_BAD_ARG;
        }
        if (st == OWCRYPT_ERR_SHORT_OUT) {
            return NETW_ERR_SHORT_OUT;
        }
        return NETW_BAN_AUTH_FAIL;
    }

    owc_put32be(e, NETW_FRAME_MAGIC);
    e[4] = NETW_FRAME_VERSION;
    owc_put64be(e + 5, seq);
    owc_put32be(e + 13, (uint32_t)(NETW_LINK_ENVELOPE + sutf_len));

    *frame_len = NETW_LINK_ENVELOPE + sutf_len;

    ln->frames_tx++;
    ln->bytes_tx += *frame_len;
    if (ln->owner != NULL) {
        ln->owner->total_frames_tx++;
    }
    return NETW_OK;
}

netwerk_status_t netwerk_open_link_frame(netwerk_link_t *ln,
                                         const uint8_t key[32],
                                         const uint8_t *frame,
                                         size_t frame_len,
                                         uint8_t *plain_out,
                                         size_t *plain_len,
                                         const uint8_t **aad_out,
                                         size_t *aad_len_out,
                                         bool scan_ban)
{
    const uint8_t  *inner_aad;
    size_t          inner_aad_len;
    uint8_t         aad[NETW_SEQ_AAD];
    uint8_t         nonce[12];
    uint64_t        seq;
    uint32_t        plen;
    size_t          min_frame;
    owcrypt_status_t st;

    if (ln == NULL || key == NULL || frame == NULL || plain_out == NULL ||
        plain_len == NULL) {
        return NETW_ERR_BAD_ARG;
    }
    if (ln->state == NETW_LINK_STATE_BANNED) {
        return NETW_ERR_STATE;
    }
    min_frame = NETW_LINK_ENVELOPE + OWCRYPT_FRAME_OVERHEAD +
                NETW_SEQ_AAD + 16u;
    if (frame_len < min_frame) {
        return NETW_BAN_ENVELOPE;
    }
    if (owc_get32be(frame) != NETW_FRAME_MAGIC ||
        frame[4] != NETW_FRAME_VERSION) {
        return NETW_BAN_ENVELOPE;
    }
    seq = owc_get64be(frame + 5);
    plen = owc_get32be(frame + 13);
    if ((size_t)plen != frame_len) {
        return NETW_BAN_ENVELOPE;
    }

    /* Pre-open replay drop: no crypto on replayed / out-of-window. */
    if (!netw_replay_ok(ln, seq)) {
        ln->replays++;
        return NETW_BAN_REPLAY;
    }

    netw_seq_aad(seq, aad);
    netwerk_link_make_nonce(seq, nonce);
    st = owcrypt_open_frame(key, nonce,
                            frame + NETW_LINK_ENVELOPE,
                            frame_len - NETW_LINK_ENVELOPE,
                            plain_out, plain_len,
                            &inner_aad, &inner_aad_len,
                            scan_ban, netw_ban_relay, ln);
    if (st != OWCRYPT_OK) {
        if (st == OWCRYPT_BAN_AUTH_FAIL) {
            ln->auth_fails++;
            if (ln->owner != NULL) {
                ln->owner->total_auth_fails++;
            }
            return NETW_BAN_AUTH_FAIL;
        }
        if (st == OWCRYPT_BAN_BANCODE) {
            ln->bans++;
            ln->state = NETW_LINK_STATE_BANNED;
            if (ln->owner != NULL) {
                ln->owner->total_bans++;
            }
            return NETW_BAN_BANCODE;
        }
        if (st == OWCRYPT_ERR_BAD_ARG) {
            return NETW_ERR_BAD_ARG;
        }
        if (st == OWCRYPT_ERR_SHORT_OUT) {
            return NETW_ERR_SHORT_OUT;
        }
        if (st == OWCRYPT_ERR_UNINITIALIZED) {
            return NETW_ERR_UNINITIALIZED;
        }
        return NETW_BAN_AUTH_FAIL;
    }
    if (inner_aad_len != NETW_SEQ_AAD ||
        !owcrypt_ct_memcmp(inner_aad, aad, NETW_SEQ_AAD)) {
        ln->auth_fails++;
        if (ln->owner != NULL) {
            ln->owner->total_auth_fails++;
        }
        return NETW_BAN_AUTH_FAIL;
    }

    netw_replay_accept(ln, seq);
    if (aad_out != NULL) { *aad_out = inner_aad; }
    if (aad_len_out != NULL) { *aad_len_out = NETW_SEQ_AAD; }

    ln->frames_rx++;
    ln->bytes_rx += frame_len;
    if (ln->owner != NULL) {
        ln->owner->total_frames_rx++;
    }
    return NETW_OK;
}