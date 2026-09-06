/*
 * netwerk_smoke.c - hosted self-test for the netwerk.owd session layer.
 *
 * Compiles DLL/netwerk/netwerk.c + DLL/owcrypt/owcrypt.c together with
 * this file against the host CRT (used only by the harness). The k64
 * API is a local facsimile with a controllable TSC so the keep-alive
 * state machine is fully deterministic.
 *
 * Runs: lifecycle, session/link registry, keep-alive heartbeat machine,
 * nonce derivation, crypto-link framing (seal/open, replay window,
 * envelope guards, tamper, ban pre-scan + ban signal hook), and a
 * link-shutdown sweep.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "netwerk.h"
#include "owcrypt.h"
#include "kernel64.h"

/* ------------------------------------------------------------------ */
/*  k64 facsimile                                                      */
/* ------------------------------------------------------------------ */

static bool fake_k64_ready = false;
static uint64_t fake_tsc = 0u;

k64_status_t k64_initialize_api(void)
{
    fake_k64_ready = true;
    return K64_OK;
}

k64_status_t k64_query_system_time(uint64_t *out_timestamp)
{
    if (out_timestamp == NULL) {
        return K64_ERR_INVALID_PARAM;
    }
    *out_timestamp = fake_tsc;
    return K64_OK;
}

/* ------------------------------------------------------------------ */
/*  helpers                                                            */
/* ------------------------------------------------------------------ */

static uint64_t be64(const uint8_t *p)
{
    uint64_t v = 0u;
    int i;
    for (i = 0; i < 8; i++) {
        v = (v << 8) | p[i];
    }
    return v;
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint32_t g_ban_bc = 0u;
static uint64_t g_ban_sc = 0u;
static void *   g_ban_ctx = NULL;
static void ban_recorder(uint32_t bancode, uint64_t subcode, void *ctx)
{
    g_ban_bc = bancode;
    g_ban_sc = subcode;
    g_ban_ctx = ctx;
}

int main(void)
{
    netwerk_session_t sess;
    netwerk_link_t   *l[NETW_MAX_LINKS];
    netwerk_link_t   *l1, *l2;
    uint8_t           key[32];
    uint8_t           buf[512];
    size_t            blen;
    uint8_t           plain[256];
    size_t            plen;
    const uint8_t    *aad;
    size_t            aadlen;
    uint8_t           nonce[12];
    uint64_t          sfx, srx;
    uint32_t          safe, sban;
    int               i;

    /* ------------------------------------------------------------------ */
    /*  Lifecycle                                                          */
    /* ------------------------------------------------------------------ */

    assert(netwerk_module_init() == NETW_OK);
    assert(netwerk_abi_version() == 0x00010000u);
    assert(netwerk_abi_major() == 1u);
    assert(netwerk_abi_minor() == 0u);
    assert(memcmp(netwerk_ident(), "netwerk.owd 1.0.0", 17u) == 0);
    assert(netwerk_link_state(NULL) == NETW_LINK_STATE_DOWN);

    /* ------------------------------------------------------------------ */
    /*  Session / link registry                                            */
    /* ------------------------------------------------------------------ */

    netwerk_session_init(&sess);
    assert(sess.initialized);
    assert(netwerk_session_link_count(&sess) == 0u);
    assert(netwerk_link_alloc(NULL, NETW_DIR_OUTBOUND, NULL, &l1)
           == NETW_ERR_BAD_ARG);
    assert(netwerk_link_alloc(&sess, (netwerk_dir_t)7u, NULL, &l1)
           == NETW_ERR_BAD_ARG);

    for (i = 0; i < 16; i++) {
        assert(netwerk_link_alloc(&sess, NETW_DIR_OUTBOUND, NULL, &l[i])
               == NETW_OK);
        assert(l[i]->id == (uint16_t)(i + 1u));
        assert(l[i]->state == NETW_LINK_STATE_SECURE);
    }
    assert(netwerk_session_link_count(&sess) == 16u);
    assert(netwerk_link_alloc(&sess, NETW_DIR_OUTBOUND, NULL, &l1)
           == NETW_ERR_LINKS_FULL);

    assert(netwerk_link_free(&sess, l[3]) == NETW_OK);      /* slot id 4 */
    assert(netwerk_session_link_count(&sess) == 15u);
    assert(netwerk_link_alloc(&sess, NETW_DIR_INBOUND, NULL, &l1) == NETW_OK);
    assert(l1 == l[3]);                                     /* reused    */
    assert(netwerk_session_find(&sess, 4u) == l1);
    assert(netwerk_session_find(&sess, 0u) == NULL);

    netwerk_session_reset(&sess);
    assert(!sess.initialized);
    netwerk_session_init(&sess);

    assert(netwerk_link_alloc(&sess, NETW_DIR_OUTBOUND, NULL, &l1) == NETW_OK);
    assert(netwerk_link_alloc(&sess, NETW_DIR_INBOUND, NULL, &l2) == NETW_OK);
    assert(l1 != l2);
    assert(netwerk_link_state(l1) == NETW_LINK_STATE_SECURE);

    l1->frames_tx = 0u;             /* silence any residual stats */

    /* ------------------------------------------------------------------ */
    /*  Keep-alive state machine (deterministic `now` ticks)               */
    /* ------------------------------------------------------------------ */

    assert(netwerk_link_set_keepalive(l1, 0u, 0u) == NETW_OK);
    assert(netwerk_link_set_keepalive(l1, 1000u, 3000u) == NETW_OK);
    assert(netwerk_link_set_keepalive(l1, 5000u, 1000u) == NETW_ERR_BAD_ARG);
    assert(netwerk_link_set_keepalive(l1, 1000u, 0u) == NETW_ERR_BAD_ARG);
    assert(netwerk_link_set_keepalive(l1, 1000u, 3000u) == NETW_OK);

    assert(netwerk_link_tx(l1, 1000u) == NETW_OK);
    assert(netwerk_link_rx(l1, 1250u) == NETW_OK);
    assert(netwerk_link_tick(l1, 2050u) == NETW_OK);        /* 0 full beats */
    assert(netwerk_link_next_beat_delta_ms(l1, 2050u) == 200u);
    assert(netwerk_link_tick(l1, 2251u) == NETW_WARN_BEATS_MISSED);
    assert(l1->missed_beats == 1u);
    assert(netwerk_link_is_alive(l1, 2251u));
    assert(netwerk_link_tick(l1, 4252u) == NETW_BAN_LINK_DEAD);
    assert(netwerk_link_state(l1) == NETW_LINK_STATE_DEAD);
    assert(!netwerk_link_is_alive(l1, 4252u));
    assert(netwerk_link_activate(l1) == NETW_OK);
    assert(netwerk_link_state(l1) == NETW_LINK_STATE_SECURE);
    assert(netwerk_link_tx(l1, 4252u) == NETW_OK);          /* respawn    */
    assert(netwerk_link_tick(l1, 4300u) == NETW_OK);
    assert(netwerk_link_is_alive(l1, 4300u));

    /* now==0 path resolves via the k64 facsimile TSC */
    fake_tsc = 999u;
    assert(netwerk_link_tx(l1, 0u) == NETW_OK);
    assert(l1->last_activity == 999u);

    /* ------------------------------------------------------------------ */
    /*  Nonce derivation                                                   */
    /* ------------------------------------------------------------------ */

    netwerk_link_make_nonce(0x1122334455667788u, nonce);
    assert(nonce[0] == 0u && nonce[1] == 0u && nonce[2] == 0u && nonce[3] == 0u);
    assert(nonce[4] == 0x88u && nonce[5] == 0x77u && nonce[6] == 0x66u &&
           nonce[7] == 0x55u && nonce[8] == 0x44u && nonce[9] == 0x33u &&
           nonce[10] == 0x22u && nonce[11] == 0x11u);

    /* ------------------------------------------------------------------ */
    /*  Crypto-link framing                                                */
    /* ------------------------------------------------------------------ */

    for (i = 0; i < 32; i++) {
        key[i] = (uint8_t)i;
    }

    /* short output buffer (need = 17 + 12 + 8 + 5 + 16 = 58) */
    blen = NETW_LINK_ENVELOPE + OWCRYPT_FRAME_OVERHEAD + 8u + 5u + 16u - 1u;
    assert(netwerk_seal_link_frame(l2, key, 1u, (const uint8_t *)"hello",
                                   5u, buf, &blen) == NETW_ERR_SHORT_OUT);

    /* seal / open seq 1 */
    {
        static const char msg[] = "Hi network world";
        blen = sizeof(buf);
        assert(netwerk_seal_link_frame(l2, key, 1u,
                                       (const uint8_t *)msg,
                                       sizeof(msg) - 1u, buf, &blen)
               == NETW_OK);
        assert(memcmp(buf, "NWL1", 4u) == 0);
        assert(buf[4] == NETW_FRAME_VERSION);
        assert(be64(buf + 5) == 1u);
        assert(be32(buf + 13) == blen);

        plen = sizeof(plain);
        assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                       &aad, &aadlen, false) == NETW_OK);
        assert(plen == sizeof(msg) - 1u);
        assert(memcmp(plain, msg, plen) == 0);
        assert(aadlen == 8u);
        assert(be64(aad) == 1u);
        assert(l2->frames_rx == 1u);
        assert(l2->bytes_rx == blen);
    }

    /* replay of the accepted frame: dropped pre-crypto */
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_BAN_REPLAY);
    assert(l2->replays == 1u);

    /* seq 2 accepted; then unseen seq 0 within window accepted */
    blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l2, key, 2u, (const uint8_t *)"two",
                                   3u, buf, &blen) == NETW_OK);
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_OK);
    blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l2, key, 0u, (const uint8_t *)"zero",
                                   4u, buf, &blen) == NETW_OK);
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_OK);
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_BAN_REPLAY);

    /* jump far ahead, then old-within-63 / old-beyond-63 */
    blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l2, key, 100u, (const uint8_t *)"jump",
                                   4u, buf, &blen) == NETW_OK);
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_OK);
    blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l2, key, 37u, (const uint8_t *)"win37",
                                   5u, buf, &blen) == NETW_OK);
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_OK);
    blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l2, key, 36u, (const uint8_t *)"old36",
                                   5u, buf, &blen) == NETW_OK);
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_BAN_REPLAY);

    /* tampered sealed region -> auth failure, link survives */
    blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l2, key, 101u, (const uint8_t *)"tat",
                                   3u, buf, &blen) == NETW_OK);
    buf[blen - 3u] ^= 0x01u;
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_BAN_AUTH_FAIL);
    assert(l2->auth_fails == 1u);
    assert(netwerk_link_state(l2) == NETW_LINK_STATE_SECURE);

    /* envelope guards */
    blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l2, key, 102u, (const uint8_t *)"env",
                                   3u, buf, &blen) == NETW_OK);
    buf[0] ^= 0xFFu;
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_BAN_ENVELOPE);
    buf[0] ^= 0xFFu;                                        /* restore   */
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, blen, plain, &plen,
                                   &aad, &aadlen, false) == NETW_OK);

    /* forged envelope seq: pre-check passes, tag check rejects */
    {
        uint8_t fbuf[512];
        size_t  f = sizeof(fbuf);
        int     k;
        assert(netwerk_seal_link_frame(l2, key, 103u,
                                       (const uint8_t *)"forge", 5u,
                                       fbuf, &f) == NETW_OK);
        for (k = 0; k < 8; k++) {
            fbuf[12 - k] = (uint8_t)(777u >> (8 * k));      /* BE(777)  */
        }
        plen = sizeof(plain);
        assert(netwerk_open_link_frame(l2, key, fbuf, f, plain, &plen,
                                       &aad, &aadlen, false)
               == NETW_BAN_AUTH_FAIL);
        (void)plen;
    }

    /* too-short frame */
    plen = sizeof(plain);
    assert(netwerk_open_link_frame(l2, key, buf, NETW_LINK_ENVELOPE + 8u,
                                   plain, &plen, &aad, &aadlen, false)
           == NETW_BAN_ENVELOPE);

    /* ------------------------------------------------------------------ */
    /*  BAN pre-scan + ban signal hook                                     */
    /* ------------------------------------------------------------------ */

    {
        static const uint8_t evil[] = { 0xaa, 0x20, 0xa3, 0x11, 0x00, 0xbb };
        netwerk_link_t *l3 = NULL, *l4 = NULL;
        assert(netwerk_link_alloc(&sess, NETW_DIR_INBOUND, NULL, &l3)
               == NETW_OK);
        assert(netwerk_link_alloc(&sess, NETW_DIR_INBOUND, NULL, &l4)
               == NETW_OK);
        assert(netwerk_link_set_ban_signal(l3, ban_recorder, l3) == NETW_OK);

        /* scan on: caught, hook fired, link banned */
        blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l3, key, 1u, evil, sizeof(evil),
                                       buf, &blen) == NETW_OK);
        plen = sizeof(plain);
        assert(netwerk_open_link_frame(l3, key, buf, blen, plain, &plen,
                                       &aad, &aadlen, true)
               == NETW_BAN_BANCODE);
        assert(g_ban_bc == 0x0011A320u);
        assert(g_ban_sc == 0u);
        assert(g_ban_ctx == l3);
        assert(l3->bans == 1u);
        assert(netwerk_link_state(l3) == NETW_LINK_STATE_BANNED);
        plen = sizeof(plain);
        assert(netwerk_open_link_frame(l3, key, buf, blen, plain, &plen,
                                       &aad, &aadlen, true)
               == NETW_ERR_STATE);
        assert(netwerk_link_tx(l3, 1u) == NETW_ERR_STATE);
        assert(netwerk_link_tick(l3, 1u) == NETW_ERR_STATE);
        assert(netwerk_link_activate(l3) == NETW_ERR_STATE);
        assert(netwerk_link_close(l3) == NETW_OK);
        assert(netwerk_link_state(l3) == NETW_LINK_STATE_DOWN);

        /* scan off: same payload passes (policy is per-open) */
        blen = sizeof(buf);
    assert(netwerk_seal_link_frame(l4, key, 1u, evil, sizeof(evil),
                                       buf, &blen) == NETW_OK);
        plen = sizeof(plain);
        assert(netwerk_open_link_frame(l4, key, buf, blen, plain, &plen,
                                       &aad, &aadlen, false) == NETW_OK);
        assert(plen == sizeof(evil));
        assert(netwerk_link_state(l4) == NETW_LINK_STATE_SECURE);
    }

    /* ------------------------------------------------------------------ */
    /*  Session totals                                                     */
    /* ------------------------------------------------------------------ */

    netwerk_session_stats(&sess, &sfx, &srx, &safe, &sban);
    assert(sfx > 0u);
    assert(srx > 0u);
    assert(safe >= 2u);                 /* tamper + forged-envelope      */
    assert(sban >= 1u);                 /* the eval-payload ban          */

    /* ------------------------------------------------------------------ */
    /*  Shutdown                                                           */
    /* ------------------------------------------------------------------ */

    netwerk_session_reset(&sess);
    assert(netwerk_module_shutdown() == NETW_OK);
    assert(netwerk_module_shutdown() == NETW_ERR_UNINITIALIZED);

    printf("netwerk_smoke: PASS\n");
    return 0;
}