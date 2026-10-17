/*
 * owtls64.h - OpenWindows TLS/Crypto Transport Layer (.owd)
 *
 * A dependency-free record-layer implementation with a pluggable byte
 * transport and a pluggable record cipher. The layer frames outbound and
 * inbound records, drives the client handshake state machine through to
 * "established", encrypts application records through the cipher hook, and
 * emits close_notify on shutdown.
 *
 * When no cipher hook is supplied a small built-in stream cipher (RC4) is
 * used, keyed per record from the session secret, so the layer produces
 * encrypted records without any external cryptographic library.
 *
 * C99 freestanding. Zero dynamic heap allocation; the caller owns the
 * (large) connection object and provides the transport callbacks.
 */

#ifndef OWTLS64_H
#define OWTLS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Protocol geometry. */
#define OWTLS_VERSION_MAJOR       3u
#define OWTLS_VERSION_MINOR       3u
#define OWTLS_RECORD_HEADER_SIZE  5u
#define OWTLS_MAX_PLAINTEXT       16384u
#define OWTLS_MAX_CIPHERTEXT      (OWTLS_MAX_PLAINTEXT + 256u)
#define OWTLS_MAX_RECORD          (OWTLS_MAX_CIPHERTEXT + OWTLS_RECORD_HEADER_SIZE)
#define OWTLS_RANDOM_SIZE         32u
#define OWTLS_MASTER_SIZE         48u
#define OWTLS_VERIFY_SIZE         12u

/* Record content types. */
#define OWTLS_CT_CHANGE_CIPHER_SPEC 20u
#define OWTLS_CT_ALERT              21u
#define OWTLS_CT_HANDSHAKE          22u
#define OWTLS_CT_APPLICATION_DATA   23u

/* Handshake message types. */
#define OWTLS_HS_CLIENT_HELLO        1u
#define OWTLS_HS_SERVER_HELLO        2u
#define OWTLS_HS_NEW_SESSION_TICKET  4u
#define OWTLS_HS_CERTIFICATE        11u
#define OWTLS_HS_SERVER_KEY_EXCHANGE 12u
#define OWTLS_HS_CERTIFICATE_REQUEST 13u
#define OWTLS_HS_SERVER_HELLO_DONE  14u
#define OWTLS_HS_CERTIFICATE_VERIFY 15u
#define OWTLS_HS_CLIENT_KEY_EXCHANGE 16u
#define OWTLS_HS_FINISHED           20u

/* Transport return: number of bytes handled (>=0), or a negative error. */
typedef struct {
    int32_t (*send)(void *ctx, const uint8_t *data, size_t len);
    int32_t (*recv)(void *ctx, uint8_t *data, size_t capacity);
    void *ctx;
} owtls_transport_t;

/*
 * Record cipher hook. Transforms `in_len` bytes into `out` (capacity
 * `out_capacity`) and stores the transformed length in `*out_len`.
 * `encrypt` selects the direction; `sequence` and `content_type` are
 * provided so the hook can derive per-record keys / nonces. `in` and `out`
 * may alias. Returns true on success.
 */
typedef struct {
    bool (*crypt)(void *ctx, bool encrypt, uint64_t sequence,
                  uint8_t content_type,
                  const uint8_t *in, size_t in_len,
                  uint8_t *out, size_t out_capacity, size_t *out_len);
    void *ctx;
} owtls_cipher_t;

/* Connection state machine. */
typedef enum {
    OWTLS_STATE_CLOSED = 0,             /* Shut down / never started      */
    OWTLS_STATE_INIT,                   /* Ready to emit ClientHello      */
    OWTLS_STATE_CLIENT_HELLO_SENT,      /* Awaiting ServerHello           */
    OWTLS_STATE_SERVER_HELLO_RECV,      /* Awaiting Certificate / Done    */
    OWTLS_STATE_SERVER_CERT_RECV,       /* Awaiting ServerHelloDone       */
    OWTLS_STATE_SERVER_DONE_RECV,       /* Awaiting server CCS/Finished   */
    OWTLS_STATE_CIPHER_SPEC_SENT,       /* Our flight sent; await Finished*/
    OWTLS_STATE_ESTABLISHED,            /* Application data may flow      */
    OWTLS_STATE_ERROR                   /* Fatal protocol/transport error */
} owtls_state_t;

/* Status codes. */
typedef uint32_t owtls_status_t;
#define OWTLS_OK            0u
#define OWTLS_WANT_READ     1u
#define OWTLS_WANT_WRITE    2u
#define OWTLS_ERR_ARG       3u
#define OWTLS_ERR_STATE     4u
#define OWTLS_ERR_TRANSPORT 5u
#define OWTLS_ERR_RECORD    6u
#define OWTLS_ERR_PROTOCOL  7u
#define OWTLS_ERR_CIPHER    8u
#define OWTLS_ERR_OVERFLOW  9u
#define OWTLS_ERR_CLOSED    10u

/* Caller-owned connection object. Contains ~34 KiB of record buffers. */
typedef struct {
    owtls_state_t            state;
    const owtls_transport_t *transport;
    owtls_cipher_t           cipher;

    uint64_t send_seq;                  /* Next outbound cipher sequence  */
    uint64_t recv_seq;                  /* Next inbound cipher sequence   */
    bool     send_encrypted;            /* Cipher active outbound         */
    bool     recv_encrypted;            /* Cipher active inbound          */
    uint32_t version;                   /* Negotiated record version      */

    uint8_t  client_random[OWTLS_RANDOM_SIZE];
    uint8_t  server_random[OWTLS_RANDOM_SIZE];
    uint8_t  premaster[OWTLS_MASTER_SIZE];
    uint8_t  master_secret[OWTLS_MASTER_SIZE];
    uint32_t rand_state;                /* Session PRNG state             */

    uint8_t  recv_buf[OWTLS_MAX_RECORD]; /* Inbound record assembly        */
    size_t   recv_len;                   /* Bytes buffered                 */
    uint32_t recv_ct_len;                /* Current record ciphertext len  */
    uint8_t  recv_type;                  /* Current record content type    */

    uint8_t  write_buf[OWTLS_MAX_RECORD]; /* Outbound record assembly      */
    size_t   tx_off;                      /* Bytes flushed                  */
    size_t   tx_len;                      /* Total bytes to flush           */
    size_t   tx_user_done;                /* User bytes already framed      */

    size_t   pending_off;                 /* Pending app payload offset     */
    size_t   pending_len;                 /* Pending app payload remaining  */
    uint8_t  flight_step;                 /* Client-flight progress 0..3    */
} owtls_conn_t;

/*
 * Initialise `conn` with `transport` and an optional record `cipher`.
 * When `cipher` is NULL (or its `crypt` is NULL) the built-in stream cipher
 * is installed. Returns OWTLS_ERR_ARG for a NULL connection or transport.
 */
owtls_status_t owtls_init(owtls_conn_t *conn,
                          const owtls_transport_t *transport,
                          const owtls_cipher_t *cipher);

/*
 * Advance the handshake. Returns OWTLS_OK once established (state
 * OWTLS_STATE_ESTABLISHED), OWTLS_WANT_READ when more peer input is
 * needed, OWTLS_WANT_WRITE when the transport is back-pressured (retry),
 * or an OWTLS_ERR_* code. Safe to call repeatedly.
 */
owtls_status_t owtls_handshake(owtls_conn_t *conn);

/*
 * Send application data, fragmenting into records. On success `*sent`
 * equals `len`. On OWTLS_WANT_WRITE the caller must retry with the SAME
 * `data`/`len`; `*sent` reports progress.
 */
owtls_status_t owtls_send(owtls_conn_t *conn, const uint8_t *data, size_t len,
                          size_t *sent);

/*
 * Receive application data into `buf` (capacity `cap`). Returns OWTLS_OK
 * with `*received` bytes (may be 0 with OWTLS_WANT_READ when no complete
 * record is available yet).
 */
owtls_status_t owtls_recv(owtls_conn_t *conn, uint8_t *buf, size_t cap,
                          size_t *received);

/* Emit close_notify (best effort) and move to OWTLS_STATE_CLOSED. */
owtls_status_t owtls_close(owtls_conn_t *conn);

#endif /* OWTLS64_H */
