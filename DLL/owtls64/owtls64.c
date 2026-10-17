/*
 * owtls64.c - OpenWindows TLS/Crypto Transport Layer (.owd)
 *
 * Record-layer framing, handshake state machine, pluggable record cipher,
 * and a built-in stream cipher for the default path.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owtls64.h"

/* ── Byte helpers ─────────────────────────────────────────────── */

static void owtls_put16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void owtls_put24(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 16);
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)v;
}

static void owtls_copy(uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t i;
    for (i = 0u; i < n; ++i) {
        dst[i] = src[i];
    }
}

/* ── Session PRNG (xorshift32) ────────────────────────────────── */

static uint32_t owtls_next_rand(owtls_conn_t *c)
{
    uint32_t x = c->rand_state;

    if (x == 0u) {
        x = 0x9E3779B9u;
    }
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    c->rand_state = x;
    return x;
}

static void owtls_random_bytes(owtls_conn_t *c, uint8_t *out, size_t n)
{
    size_t i;
    for (i = 0u; i < n; ++i) {
        if ((i & 3u) == 0u) {
            uint32_t r = owtls_next_rand(c);
            out[i] = (uint8_t)r;
            if (i + 1u < n) { out[i + 1u] = (uint8_t)(r >> 8); }
            if (i + 2u < n) { out[i + 2u] = (uint8_t)(r >> 16); }
            if (i + 3u < n) { out[i + 3u] = (uint8_t)(r >> 24); }
        }
    }
}

/* ── Built-in stream cipher (RC4) ─────────────────────────────── */

typedef struct {
    uint8_t s[256];
    uint8_t i;
    uint8_t j;
} owtls_rc4_t;

static void owtls_rc4_init(owtls_rc4_t *r, const uint8_t *key, size_t key_len)
{
    uint32_t i;
    uint8_t j = 0u;

    for (i = 0u; i < 256u; ++i) {
        r->s[i] = (uint8_t)i;
    }
    for (i = 0u; i < 256u; ++i) {
        uint8_t t;
        j = (uint8_t)(j + r->s[i] + key[i % key_len]);
        t = r->s[i];
        r->s[i] = r->s[j];
        r->s[j] = t;
    }
    r->i = 0u;
    r->j = 0u;
}

static uint8_t owtls_rc4_byte(owtls_rc4_t *r)
{
    uint8_t t;

    r->i = (uint8_t)(r->i + 1u);
    r->j = (uint8_t)(r->j + r->s[r->i]);
    t = r->s[r->i];
    r->s[r->i] = r->s[r->j];
    r->s[r->j] = t;
    return r->s[(uint8_t)(r->s[r->i] + r->s[r->j])];
}

/*
 * Derive a per-record key from the session secret, the record sequence,
 * and the content type, then XOR the keystream over the payload. The
 * same routine performs both encryption and decryption.
 */
static bool owtls_default_cipher(void *ctx, bool encrypt, uint64_t sequence,
                                 uint8_t content_type,
                                 const uint8_t *in, size_t in_len,
                                 uint8_t *out, size_t out_capacity,
                                 size_t *out_len)
{
    owtls_conn_t *c = (owtls_conn_t *)ctx;
    uint8_t key[OWTLS_MASTER_SIZE + 9u];
    owtls_rc4_t rc4;
    size_t i;

    (void)encrypt;

    if (!c || !out_len || !in || !out) {
        return false;
    }
    if (in_len > out_capacity) {
        return false;
    }

    owtls_copy(key, c->master_secret, OWTLS_MASTER_SIZE);
    for (i = 0u; i < 8u; ++i) {
        key[OWTLS_MASTER_SIZE + i] = (uint8_t)(sequence >> (i * 8u));
    }
    key[OWTLS_MASTER_SIZE + 8u] = content_type;

    owtls_rc4_init(&rc4, key, sizeof(key));
    for (i = 0u; i < in_len; ++i) {
        out[i] = (uint8_t)(in[i] ^ owtls_rc4_byte(&rc4));
    }
    *out_len = in_len;
    return true;
}

/* ── Key derivation (built-in PRF) ────────────────────────────── */

/*
 * Expand `secret` into `out` using the RC4 keystream keyed by
 * secret || seed. This is the built-in KDF used only on the default path;
 * a caller-supplied cipher hook may replace the whole cryptographic flow.
 */
static void owtls_prf(const uint8_t *secret, size_t secret_len,
                      const uint8_t *seed, size_t seed_len,
                      uint8_t *out, size_t out_len)
{
    uint8_t key[OWTLS_MASTER_SIZE + 64u];
    owtls_rc4_t rc4;
    size_t i;

    if (secret_len > OWTLS_MASTER_SIZE) {
        secret_len = OWTLS_MASTER_SIZE;
    }
    if (seed_len > 64u) {
        seed_len = 64u;
    }
    owtls_copy(key, secret, secret_len);
    if (seed_len > 0u) {
        owtls_copy(key + secret_len, seed, seed_len);
    }
    owtls_rc4_init(&rc4, key, secret_len + seed_len);
    /* Discard an initial keystream block before producing output. */
    for (i = 0u; i < 16u; ++i) {
        (void)owtls_rc4_byte(&rc4);
    }
    for (i = 0u; i < out_len; ++i) {
        out[i] = owtls_rc4_byte(&rc4);
    }
}

static void owtls_derive_master(owtls_conn_t *c)
{
    uint8_t seed[OWTLS_RANDOM_SIZE * 2u];

    owtls_copy(seed, c->server_random, OWTLS_RANDOM_SIZE);
    owtls_copy(seed + OWTLS_RANDOM_SIZE, c->client_random, OWTLS_RANDOM_SIZE);
    owtls_prf(c->premaster, OWTLS_MASTER_SIZE, seed, sizeof(seed),
              c->master_secret, OWTLS_MASTER_SIZE);
}

/* ── Record cipher dispatch ───────────────────────────────────── */

static bool owtls_crypt(owtls_conn_t *c, bool encrypt, uint8_t type,
                        const uint8_t *in, size_t in_len,
                        uint8_t *out, size_t out_capacity, size_t *out_len)
{
    uint64_t seq = encrypt ? c->send_seq : c->recv_seq;

    if (!c->cipher.crypt) {
        return false;
    }
    return c->cipher.crypt(c->cipher.ctx, encrypt, seq, type,
                           in, in_len, out, out_capacity, out_len);
}

/* ── Outbound records ─────────────────────────────────────────── */

static owtls_status_t owtls_flush(owtls_conn_t *c)
{
    while (c->tx_off < c->tx_len) {
        int32_t n = c->transport->send(c->transport->ctx,
                                       c->write_buf + c->tx_off,
                                       c->tx_len - c->tx_off);
        if (n < 0) {
            return OWTLS_ERR_TRANSPORT;
        }
        if (n == 0) {
            return OWTLS_WANT_WRITE;
        }
        c->tx_off += (size_t)n;
    }
    c->tx_off = 0u;
    c->tx_len = 0u;
    return OWTLS_OK;
}

/*
 * Frame and (when requested) encrypt one record, then flush it. A record
 * already pending from a previous WANT_WRITE is flushed without being
 * rebuilt, so callers can safely retry.
 */
static owtls_status_t owtls_write_record(owtls_conn_t *c, uint8_t type,
                                         const uint8_t *payload, size_t len,
                                         bool encrypt)
{
    if (c->tx_len == 0u) {
        size_t ct_len = len;

        if (len > OWTLS_MAX_PLAINTEXT) {
            return OWTLS_ERR_OVERFLOW;
        }
        if (encrypt) {
            size_t produced = 0u;

            if (!owtls_crypt(c, true, type, payload, len,
                             c->write_buf + OWTLS_RECORD_HEADER_SIZE,
                             sizeof(c->write_buf) - OWTLS_RECORD_HEADER_SIZE,
                             &produced)) {
                return OWTLS_ERR_CIPHER;
            }
            ct_len = produced;
            c->send_seq++;
        } else {
            owtls_copy(c->write_buf + OWTLS_RECORD_HEADER_SIZE, payload, len);
        }

        if (ct_len > OWTLS_MAX_CIPHERTEXT) {
            return OWTLS_ERR_OVERFLOW;
        }
        c->write_buf[0] = type;
        c->write_buf[1] = (uint8_t)OWTLS_VERSION_MAJOR;
        c->write_buf[2] = (uint8_t)OWTLS_VERSION_MINOR;
        owtls_put16(c->write_buf + 3, (uint16_t)ct_len);
        c->tx_off = 0u;
        c->tx_len = OWTLS_RECORD_HEADER_SIZE + ct_len;
    }

    return owtls_flush(c);
}

/* ── Inbound records ──────────────────────────────────────────── */

/* Ensure a full record is buffered; writes the payload length to *out_len. */
static owtls_status_t owtls_fill_record(owtls_conn_t *c, size_t *out_len)
{
    for (;;) {
        if (c->recv_len >= (size_t)OWTLS_RECORD_HEADER_SIZE) {
            uint32_t plen = ((uint32_t)c->recv_buf[3] << 8) |
                            (uint32_t)c->recv_buf[4];

            if (plen > (uint32_t)OWTLS_MAX_CIPHERTEXT) {
                return OWTLS_ERR_RECORD;
            }
            if (c->recv_len >= (size_t)OWTLS_RECORD_HEADER_SIZE + (size_t)plen) {
                *out_len = (size_t)plen;
                return OWTLS_OK;
            }
        }

        {
            int32_t got = c->transport->recv(c->transport->ctx,
                                             c->recv_buf + c->recv_len,
                                             sizeof(c->recv_buf) - c->recv_len);
            if (got < 0) {
                return OWTLS_ERR_TRANSPORT;
            }
            if (got == 0) {
                return OWTLS_WANT_READ;
            }
            c->recv_len += (size_t)got;
        }
    }
}

/* Drop the current record from the inbound buffer. */
static void owtls_consume(owtls_conn_t *c)
{
    size_t total = (size_t)OWTLS_RECORD_HEADER_SIZE + (size_t)c->recv_ct_len;
    size_t remain;
    size_t i;

    if (total > c->recv_len) {
        c->recv_len = 0u;
        return;
    }
    remain = c->recv_len - total;
    for (i = 0u; i < remain; ++i) {
        c->recv_buf[i] = c->recv_buf[total + i];
    }
    c->recv_len = remain;
}

/*
 * Retrieve the next record, decrypting in place when the inbound cipher is
 * active. The payload is NOT consumed; call owtls_consume() afterwards.
 */
static owtls_status_t owtls_read_record(owtls_conn_t *c, uint8_t *out_type,
                                        const uint8_t **out_payload,
                                        size_t *out_len)
{
    size_t plen = 0u;
    owtls_status_t st = owtls_fill_record(c, &plen);

    if (st != OWTLS_OK) {
        return st;
    }

    c->recv_ct_len = (uint32_t)plen;
    c->recv_type = c->recv_buf[0];

    if (c->recv_encrypted) {
        size_t produced = 0u;

        if (!owtls_crypt(c, false, c->recv_type,
                         c->recv_buf + OWTLS_RECORD_HEADER_SIZE, plen,
                         c->recv_buf + OWTLS_RECORD_HEADER_SIZE, plen,
                         &produced)) {
            return OWTLS_ERR_CIPHER;
        }
        c->recv_seq++;
        *out_len = produced;
    } else {
        *out_len = plen;
    }

    *out_type = c->recv_type;
    *out_payload = c->recv_buf + OWTLS_RECORD_HEADER_SIZE;
    return OWTLS_OK;
}

/* ── Handshake message handling ───────────────────────────────── */

static owtls_status_t owtls_send_client_hello(owtls_conn_t *c)
{
    uint8_t rec[4u + 41u];
    uint8_t *b = rec + 4u;

    rec[0] = OWTLS_HS_CLIENT_HELLO;
    owtls_put24(rec + 1, 41u);

    b[0] = (uint8_t)OWTLS_VERSION_MAJOR;
    b[1] = (uint8_t)OWTLS_VERSION_MINOR;
    owtls_copy(b + 2u, c->client_random, OWTLS_RANDOM_SIZE);

    b[34] = 0u;                       /* session_id length           */
    owtls_put16(b + 35u, 2u);         /* cipher_suites length        */
    owtls_put16(b + 37u, 0x002Fu);    /* TLS_RSA_WITH_AES_128_CBC_SHA */
    b[39] = 1u;                       /* compression_methods length  */
    b[40] = 0u;                       /* null compression            */

    return owtls_write_record(c, OWTLS_CT_HANDSHAKE, rec, sizeof(rec), false);
}

static owtls_status_t owtls_send_client_flight(owtls_conn_t *c)
{
    if (c->flight_step == 0u) {
        uint8_t cke[4u + OWTLS_MASTER_SIZE];

        cke[0] = OWTLS_HS_CLIENT_KEY_EXCHANGE;
        owtls_put24(cke + 1, OWTLS_MASTER_SIZE);
        owtls_copy(cke + 4u, c->premaster, OWTLS_MASTER_SIZE);

        {
            owtls_status_t st = owtls_write_record(c, OWTLS_CT_HANDSHAKE,
                                                   cke, sizeof(cke), false);
            if (st != OWTLS_OK) {
                return st;
            }
        }
        c->flight_step = 1u;
    }

    if (c->flight_step == 1u) {
        uint8_t ccs[1];

        ccs[0] = 0x01u;
        {
            owtls_status_t st = owtls_write_record(c, OWTLS_CT_CHANGE_CIPHER_SPEC,
                                                   ccs, 1u, false);
            if (st != OWTLS_OK) {
                return st;
            }
        }
        c->send_encrypted = true;
        c->flight_step = 2u;
    }

    if (c->flight_step == 2u) {
        uint8_t fin[4u + OWTLS_VERIFY_SIZE];
        owtls_status_t st;

        fin[0] = OWTLS_HS_FINISHED;
        owtls_put24(fin + 1, OWTLS_VERIFY_SIZE);
        owtls_prf(c->master_secret, OWTLS_MASTER_SIZE,
                  c->client_random, OWTLS_RANDOM_SIZE,
                  fin + 4u, OWTLS_VERIFY_SIZE);

        st = owtls_write_record(c, OWTLS_CT_HANDSHAKE, fin, sizeof(fin), true);
        if (st != OWTLS_OK) {
            return st;
        }
        c->flight_step = 3u;
        c->state = OWTLS_STATE_CIPHER_SPEC_SENT;
    }

    return OWTLS_OK;
}

static owtls_status_t owtls_handle_alert(owtls_conn_t *c,
                                         const uint8_t *p, size_t len)
{
    uint8_t level;
    uint8_t desc;

    if (len < 2u) {
        return OWTLS_ERR_PROTOCOL;
    }
    level = p[0];
    desc = p[1];

    if (desc == 0u) {                       /* close_notify */
        c->state = OWTLS_STATE_CLOSED;
        return OWTLS_ERR_CLOSED;
    }
    if (level == 2u) {                      /* fatal */
        c->state = OWTLS_STATE_ERROR;
        return OWTLS_ERR_PROTOCOL;
    }
    return OWTLS_OK;
}

static owtls_status_t owtls_handle_handshake(owtls_conn_t *c, uint8_t type,
                                             const uint8_t *body, size_t len)
{
    switch (type) {
    case OWTLS_HS_SERVER_HELLO:
        if (c->state != OWTLS_STATE_CLIENT_HELLO_SENT || len < 38u) {
            return OWTLS_ERR_PROTOCOL;
        }
        c->version = ((uint32_t)body[0] << 8) | (uint32_t)body[1];
        owtls_copy(c->server_random, body + 2u, OWTLS_RANDOM_SIZE);
        owtls_derive_master(c);
        c->state = OWTLS_STATE_SERVER_HELLO_RECV;
        return OWTLS_OK;

    case OWTLS_HS_CERTIFICATE:
        if (c->state != OWTLS_STATE_SERVER_HELLO_RECV) {
            return OWTLS_ERR_PROTOCOL;
        }
        c->state = OWTLS_STATE_SERVER_CERT_RECV;
        return OWTLS_OK;

    case OWTLS_HS_SERVER_KEY_EXCHANGE:
    case OWTLS_HS_CERTIFICATE_REQUEST:
    case OWTLS_HS_NEW_SESSION_TICKET:
        return OWTLS_OK;

    case OWTLS_HS_SERVER_HELLO_DONE:
        if (c->state != OWTLS_STATE_SERVER_HELLO_RECV &&
            c->state != OWTLS_STATE_SERVER_CERT_RECV) {
            return OWTLS_ERR_PROTOCOL;
        }
        c->state = OWTLS_STATE_SERVER_DONE_RECV;
        return owtls_send_client_flight(c);

    case OWTLS_HS_FINISHED:
        if (c->state != OWTLS_STATE_CIPHER_SPEC_SENT) {
            return OWTLS_ERR_PROTOCOL;
        }
        c->state = OWTLS_STATE_ESTABLISHED;
        return OWTLS_OK;

    default:
        return OWTLS_ERR_PROTOCOL;
    }
}

/* Process a handshake record that may hold one or more messages. */
static owtls_status_t owtls_process_handshake(owtls_conn_t *c,
                                              const uint8_t *p, size_t len)
{
    size_t off = 0u;

    while (off + 4u <= len) {
        uint8_t type = p[off];
        size_t hlen = ((size_t)p[off + 1u] << 16) |
                      ((size_t)p[off + 2u] << 8) |
                       (size_t)p[off + 3u];
        owtls_status_t st;

        if (off + 4u + hlen > len) {
            return OWTLS_ERR_PROTOCOL;   /* Fragmented handshakes unsupported */
        }
        st = owtls_handle_handshake(c, type, p + off + 4u, hlen);
        if (st != OWTLS_OK) {
            return st;
        }
        off += 4u + hlen;
    }

    if (off != len) {
        return OWTLS_ERR_PROTOCOL;
    }
    return OWTLS_OK;
}

/* ── Public API ───────────────────────────────────────────────── */

owtls_status_t owtls_init(owtls_conn_t *conn,
                          const owtls_transport_t *transport,
                          const owtls_cipher_t *cipher)
{
    size_t i;

    if (!conn || !transport || !transport->send || !transport->recv) {
        return OWTLS_ERR_ARG;
    }

    {
        uint8_t *raw = (uint8_t *)conn;
        for (i = 0u; i < sizeof(*conn); ++i) {
            raw[i] = 0u;
        }
    }

    conn->transport = transport;
    if (cipher && cipher->crypt) {
        conn->cipher = *cipher;
    } else {
        conn->cipher.crypt = owtls_default_cipher;
        conn->cipher.ctx = conn;
    }

    conn->state = OWTLS_STATE_INIT;
    conn->version = ((uint32_t)OWTLS_VERSION_MAJOR << 8) |
                    (uint32_t)OWTLS_VERSION_MINOR;
    conn->rand_state = (uint32_t)(uintptr_t)conn ^ 0xA5A5A5A5u;

    owtls_random_bytes(conn, conn->client_random, OWTLS_RANDOM_SIZE);
    owtls_random_bytes(conn, conn->premaster, OWTLS_MASTER_SIZE);

    return OWTLS_OK;
}

owtls_status_t owtls_handshake(owtls_conn_t *conn)
{
    if (!conn) {
        return OWTLS_ERR_ARG;
    }
    if (conn->state == OWTLS_STATE_ESTABLISHED) {
        return OWTLS_OK;
    }
    if (conn->state == OWTLS_STATE_CLOSED || conn->state == OWTLS_STATE_ERROR) {
        return OWTLS_ERR_STATE;
    }

    if (conn->state == OWTLS_STATE_INIT) {
        owtls_status_t st = owtls_send_client_hello(conn);

        if (st != OWTLS_OK) {
            return st;
        }
        conn->state = OWTLS_STATE_CLIENT_HELLO_SENT;
    }

    for (;;) {
        uint8_t type = 0u;
        const uint8_t *p = NULL;
        size_t len = 0u;
        owtls_status_t st;

        if (conn->flight_step > 0u && conn->flight_step < 3u) {
            st = owtls_send_client_flight(conn);
            if (st != OWTLS_OK) {
                return st;
            }
        }

        st = owtls_read_record(conn, &type, &p, &len);
        if (st == OWTLS_WANT_READ) {
            return OWTLS_WANT_READ;
        }
        if (st != OWTLS_OK) {
            conn->state = OWTLS_STATE_ERROR;
            return st;
        }

        if (type == OWTLS_CT_CHANGE_CIPHER_SPEC) {
            conn->recv_encrypted = true;
            owtls_consume(conn);
            continue;
        }
        if (type == OWTLS_CT_ALERT) {
            st = owtls_handle_alert(conn, p, len);
            owtls_consume(conn);
            return st;
        }
        if (type == OWTLS_CT_HANDSHAKE) {
            st = owtls_process_handshake(conn, p, len);
            owtls_consume(conn);
            if (st == OWTLS_WANT_WRITE || st == OWTLS_WANT_READ) {
                return st;
            }
            if (st != OWTLS_OK) {
                conn->state = OWTLS_STATE_ERROR;
                return st;
            }
            if (conn->state == OWTLS_STATE_ESTABLISHED) {
                return OWTLS_OK;
            }
            continue;
        }

        owtls_consume(conn);
        conn->state = OWTLS_STATE_ERROR;
        return OWTLS_ERR_PROTOCOL;
    }
}

static owtls_status_t owtls_drain_app(owtls_conn_t *c, uint8_t *buf,
                                      size_t cap, size_t *received)
{
    size_t n;
    size_t i;

    n = (c->pending_len < cap) ? c->pending_len : cap;
    for (i = 0u; i < n; ++i) {
        buf[i] = c->recv_buf[c->pending_off + i];
    }
    c->pending_off += n;
    c->pending_len -= n;
    *received = n;

    if (c->pending_len == 0u) {
        owtls_consume(c);
    }
    return OWTLS_OK;
}

owtls_status_t owtls_send(owtls_conn_t *conn, const uint8_t *data, size_t len,
                          size_t *sent)
{
    if (!conn || !sent || (!data && len != 0u)) {
        return OWTLS_ERR_ARG;
    }
    *sent = 0u;

    if (conn->state != OWTLS_STATE_ESTABLISHED) {
        return OWTLS_ERR_STATE;
    }

    while (conn->tx_user_done < len) {
        owtls_status_t st;
        size_t frag;

        if (conn->tx_len > 0u) {
            st = owtls_flush(conn);
            *sent = conn->tx_user_done;
            if (st != OWTLS_OK) {
                return st;
            }
        }

        frag = len - conn->tx_user_done;
        if (frag > (size_t)OWTLS_MAX_PLAINTEXT) {
            frag = (size_t)OWTLS_MAX_PLAINTEXT;
        }

        st = owtls_write_record(conn, OWTLS_CT_APPLICATION_DATA,
                                data + conn->tx_user_done, frag, true);
        conn->tx_user_done += frag;
        *sent = conn->tx_user_done;
        if (st != OWTLS_OK) {
            if (st == OWTLS_ERR_CIPHER || st == OWTLS_ERR_OVERFLOW) {
                conn->tx_user_done = 0u;
            }
            return st;
        }
    }

    *sent = len;
    conn->tx_user_done = 0u;
    return OWTLS_OK;
}

owtls_status_t owtls_recv(owtls_conn_t *conn, uint8_t *buf, size_t cap,
                          size_t *received)
{
    if (!conn || !buf || !received) {
        return OWTLS_ERR_ARG;
    }
    *received = 0u;

    if (conn->state != OWTLS_STATE_ESTABLISHED) {
        return OWTLS_ERR_STATE;
    }

    if (conn->pending_len > 0u) {
        return owtls_drain_app(conn, buf, cap, received);
    }

    for (;;) {
        uint8_t type = 0u;
        const uint8_t *p = NULL;
        size_t len = 0u;
        owtls_status_t st;

        st = owtls_read_record(conn, &type, &p, &len);
        if (st == OWTLS_WANT_READ) {
            return OWTLS_WANT_READ;
        }
        if (st != OWTLS_OK) {
            return st;
        }

        if (type == OWTLS_CT_APPLICATION_DATA) {
            conn->pending_off = OWTLS_RECORD_HEADER_SIZE;
            conn->pending_len = len;
            return owtls_drain_app(conn, buf, cap, received);
        }
        if (type == OWTLS_CT_ALERT) {
            st = owtls_handle_alert(conn, p, len);
            owtls_consume(conn);
            return st;
        }
        if (type == OWTLS_CT_HANDSHAKE) {
            st = owtls_process_handshake(conn, p, len);
            owtls_consume(conn);
            if (st != OWTLS_OK) {
                return st;
            }
            continue;
        }
        if (type == OWTLS_CT_CHANGE_CIPHER_SPEC) {
            conn->recv_encrypted = true;
            owtls_consume(conn);
            continue;
        }

        owtls_consume(conn);
        return OWTLS_ERR_PROTOCOL;
    }
}

owtls_status_t owtls_close(owtls_conn_t *conn)
{
    owtls_status_t st = OWTLS_OK;

    if (!conn) {
        return OWTLS_ERR_ARG;
    }
    if (conn->state == OWTLS_STATE_CLOSED) {
        return OWTLS_OK;
    }

    if (conn->transport && conn->state == OWTLS_STATE_ESTABLISHED) {
        uint8_t alert[2];

        alert[0] = 1u;   /* warning        */
        alert[1] = 0u;   /* close_notify   */
        st = owtls_write_record(conn, OWTLS_CT_ALERT, alert, 2u,
                                conn->send_encrypted);
    }

    conn->state = OWTLS_STATE_CLOSED;
    conn->pending_len = 0u;
    conn->tx_user_done = 0u;
    return st;
}
