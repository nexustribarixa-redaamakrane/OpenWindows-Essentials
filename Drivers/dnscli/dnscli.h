/*
 * dnscli.h - OpenWindows DNS Resolver (.owc)
 *
 * Recursive A-record resolver: builds a QNAME-encoded query with a
 * transaction id, transmits it through a pluggable UDP hook, parses A
 * records from the response, and caches name -> address mappings in a
 * fixed static table.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef DNSCLI_H
#define DNSCLI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t dns_status_t;

#define DNS_OK            0x00000000u  /* Resolved / completed          */
#define DNS_ERR_PARAM     0x00000001u  /* Invalid argument              */
#define DNS_ERR_NOT_READY 0x00000002u  /* Resolver not initialised      */
#define DNS_ERR_NO_SERVER 0x00000003u  /* No name server configured     */
#define DNS_ERR_NAME      0x00000004u  /* Name cannot be encoded        */
#define DNS_ERR_PENDING   0x00000005u  /* Query sent, awaiting response */
#define DNS_ERR_TRUNCATED 0x00000006u  /* TC bit set in response        */
#define DNS_ERR_RCODE     0x00000007u  /* Server returned an error      */
#define DNS_ERR_NXDOMAIN  0x00000008u  /* Name does not exist           */
#define DNS_ERR_NO_DATA   0x00000009u  /* No A record in response/cache */
#define DNS_ERR_BAD_PACKET 0x0000000Au /* Malformed message             */

/* ── Protocol constants ───────────────────────────────────────── */

#define DNS_PORT       53u
#define DNS_TYPE_A     1u
#define DNS_CLASS_IN   1u
#define DNS_HEADER_LEN 12u

/* ── Capacity limits (static storage) ─────────────────────────── */

#define DNS_MAX_NAME  128u
#define DNS_CACHE_MAX 16u
#define DNS_MSG_MAX   512u

/* ── UDP transmit hook ────────────────────────────────────────── */

typedef dns_status_t (*dns_udp_tx_fn)(void *ctx, uint32_t dst_ip,
                                      uint16_t dst_port,
                                      const void *payload, uint32_t len);

/* ── Cache entry ──────────────────────────────────────────────── */

typedef struct {
    char     name[DNS_MAX_NAME];
    uint32_t ip;
    uint32_t ttl;
    uint32_t age;
    bool     valid;
} dns_cache_entry_t;

/* ── Resolver instance (caller-provided, zero heap) ───────────── */

typedef struct {
    uint32_t server_ip;
    uint16_t server_port;
    uint16_t next_id;
    uint16_t id_seed;
    bool     initialized;
    dns_udp_tx_fn udp_tx;
    void    *udp_ctx;
    uint16_t pending_id;
    uint16_t reserved;
    char     pending_name[DNS_MAX_NAME];
    dns_cache_entry_t cache[DNS_CACHE_MAX];
    uint8_t  scratch[DNS_MSG_MAX];
} dns_resolver_t;

/* ── API ──────────────────────────────────────────────────────── */

dns_status_t dns_init(dns_resolver_t *res, uint32_t server_ip,
                      dns_udp_tx_fn tx, void *tx_ctx);
dns_status_t dns_resolve(dns_resolver_t *res, const char *name,
                         uint32_t *out_ip);
dns_status_t dns_cache_flush(dns_resolver_t *res);
dns_status_t dns_set_server(dns_resolver_t *res, uint32_t server_ip);

/* ── Integration helpers ──────────────────────────────────────── */

dns_status_t dns_input(dns_resolver_t *res, const void *msg, uint32_t len);
dns_status_t dns_cache_lookup(const dns_resolver_t *res, const char *name,
                              uint32_t *out_ip);

#endif /* DNSCLI_H */
