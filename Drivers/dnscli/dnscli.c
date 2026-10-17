/*
 * dnscli.c - OpenWindows DNS Resolver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "dnscli.h"

/* ── Local helpers ────────────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    size_t i;
    for (i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

static void ow_memcpy(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static size_t ow_strlen(const char *s)
{
    size_t n = 0u;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

/* Case-insensitive ASCII comparison (DNS names are case-insensitive). */
static bool ow_streq_ci(const char *a, const char *b)
{
    size_t i = 0u;
    for (;;) {
        char ca = a[i];
        char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return false;
        }
        if (ca == '\0') {
            return true;
        }
        i++;
    }
}

/* ── Byte-order helpers ───────────────────────────────────────── */

static inline uint16_t dns_rd16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | (uint16_t)p[1]);
}

static inline uint32_t dns_rd32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

static inline void dns_wr16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

/* A-record rdata shares the IP stack's byte layout. */
static inline uint32_t dns_get_ip(const uint8_t *p)
{
    uint32_t ip;
    ow_memcpy(&ip, p, 4u);
    return ip;
}

/* ── QNAME encoding ───────────────────────────────────────────── */

/* Encode a dotted name into length-prefixed labels. Returns the
 * encoded length including the root terminator, or 0 on error. */
static uint32_t dns_encode_name(const char *name, uint8_t *dst,
                                uint32_t cap)
{
    uint32_t out = 0u;
    uint32_t i = 0u;

    if (name == NULL || name[0] == '\0') {
        return 0u;
    }
    for (;;) {
        uint32_t label_pos;
        uint32_t label_len;

        if (out >= cap) {
            return 0u;
        }
        label_pos = out;
        out += 1u;               /* reserve the length octet */
        label_len = 0u;

        while (name[i] != '\0' && name[i] != '.') {
            if (label_len >= 63u || out >= cap) {
                return 0u;
            }
            dst[out++] = (uint8_t)name[i];
            label_len++;
            i++;
        }
        if (label_len == 0u) {
            return 0u;           /* empty label */
        }
        dst[label_pos] = (uint8_t)label_len;

        if (name[i] == '\0') {
            break;
        }
        i++;                     /* skip the dot */
        if (name[i] == '\0') {
            return 0u;           /* trailing dot not supported */
        }
    }

    if (out >= cap) {
        return 0u;
    }
    dst[out++] = 0u;             /* root terminator */
    if (out > 255u) {
        return 0u;
    }
    return out;
}

/* Advance past a (possibly compressed) name. */
static bool dns_skip_name(const uint8_t *msg, uint32_t len, uint32_t *off)
{
    uint32_t p = *off;

    while (p < len) {
        uint8_t c = msg[p];
        if ((c & 0xC0u) == 0xC0u) {          /* compression pointer */
            if ((p + 1u) >= len) {
                return false;
            }
            *off = p + 2u;
            return true;
        }
        if ((c & 0xC0u) != 0u) {             /* reserved label type  */
            return false;
        }
        if (c == 0u) {
            *off = p + 1u;
            return true;
        }
        p += (uint32_t)c + 1u;
    }
    return false;
}

/* ── Cache ────────────────────────────────────────────────────── */

static void dns_cache_insert(dns_resolver_t *res, const char *name,
                             uint32_t ip, uint32_t ttl)
{
    uint32_t i;
    uint32_t victim = 0u;
    uint32_t oldest = 0u;
    size_t n;

    for (i = 0u; i < DNS_CACHE_MAX; ++i) {
        if (res->cache[i].valid &&
            ow_streq_ci(res->cache[i].name, name)) {
            res->cache[i].ip  = ip;
            res->cache[i].ttl = ttl;
            res->cache[i].age = 0u;
            return;
        }
    }
    for (i = 0u; i < DNS_CACHE_MAX; ++i) {
        if (!res->cache[i].valid) {
            victim = i;
            goto fill;
        }
    }
    for (i = 0u; i < DNS_CACHE_MAX; ++i) {
        if (res->cache[i].age >= oldest) {
            oldest = res->cache[i].age;
            victim = i;
        }
    }

fill:
    n = ow_strlen(name);
    if (n >= DNS_MAX_NAME) {
        n = DNS_MAX_NAME - 1u;
    }
    ow_memcpy(res->cache[victim].name, name, n);
    res->cache[victim].name[n] = '\0';
    res->cache[victim].ip    = ip;
    res->cache[victim].ttl   = ttl;
    res->cache[victim].age   = 0u;
    res->cache[victim].valid = true;
}

dns_status_t dns_cache_lookup(const dns_resolver_t *res, const char *name,
                              uint32_t *out_ip)
{
    uint32_t i;

    if (res == NULL || name == NULL || out_ip == NULL) {
        return DNS_ERR_PARAM;
    }
    for (i = 0u; i < DNS_CACHE_MAX; ++i) {
        if (res->cache[i].valid &&
            ow_streq_ci(res->cache[i].name, name)) {
            *out_ip = res->cache[i].ip;
            return DNS_OK;
        }
    }
    return DNS_ERR_NO_DATA;
}

/* ── Public API ───────────────────────────────────────────────── */

dns_status_t dns_init(dns_resolver_t *res, uint32_t server_ip,
                      dns_udp_tx_fn tx, void *tx_ctx)
{
    if (res == NULL || tx == NULL) {
        return DNS_ERR_PARAM;
    }
    ow_memset(res, 0, sizeof(*res));
    res->server_ip   = server_ip;
    res->server_port = DNS_PORT;
    res->udp_tx      = tx;
    res->udp_ctx     = tx_ctx;
    res->id_seed     = 0x4E21u;
    res->next_id     = res->id_seed;
    res->initialized = true;
    return DNS_OK;
}

dns_status_t dns_set_server(dns_resolver_t *res, uint32_t server_ip)
{
    if (res == NULL) {
        return DNS_ERR_PARAM;
    }
    res->server_ip = server_ip;
    return DNS_OK;
}

dns_status_t dns_cache_flush(dns_resolver_t *res)
{
    if (res == NULL) {
        return DNS_ERR_PARAM;
    }
    ow_memset(res->cache, 0, sizeof(res->cache));
    return DNS_OK;
}

dns_status_t dns_resolve(dns_resolver_t *res, const char *name,
                         uint32_t *out_ip)
{
    uint32_t cached;
    uint32_t name_len;
    uint32_t off;
    dns_status_t rc;
    size_t n;

    if (res == NULL || name == NULL || out_ip == NULL) {
        return DNS_ERR_PARAM;
    }
    if (!res->initialized) {
        return DNS_ERR_NOT_READY;
    }
    if (res->server_ip == 0u) {
        return DNS_ERR_NO_SERVER;
    }

    rc = dns_cache_lookup(res, name, &cached);
    if (rc == DNS_OK) {
        *out_ip = cached;
        return DNS_OK;
    }
    if (ow_strlen(name) >= DNS_MAX_NAME) {
        return DNS_ERR_NAME;
    }

    /* Header: id, flags (RD), qdcount = 1. */
    ow_memset(res->scratch, 0, DNS_HEADER_LEN);
    res->pending_id = res->next_id;
    dns_wr16(res->scratch, res->next_id);
    res->next_id = (uint16_t)(res->next_id + 1u);
    dns_wr16(res->scratch + 2u, 0x0100u);
    dns_wr16(res->scratch + 4u, 1u);

    name_len = dns_encode_name(name,
                               res->scratch + DNS_HEADER_LEN,
                               (uint32_t)(DNS_MSG_MAX - DNS_HEADER_LEN - 4u));
    if (name_len == 0u) {
        return DNS_ERR_NAME;
    }
    off = DNS_HEADER_LEN + name_len;
    dns_wr16(res->scratch + off, DNS_TYPE_A);
    off += 2u;
    dns_wr16(res->scratch + off, DNS_CLASS_IN);
    off += 2u;

    /* Remember which name the outstanding transaction resolves. */
    n = ow_strlen(name);
    ow_memcpy(res->pending_name, name, n);
    res->pending_name[n] = '\0';

    rc = res->udp_tx(res->udp_ctx, res->server_ip, res->server_port,
                     res->scratch, off);
    if (rc != DNS_OK) {
        return rc;
    }
    return DNS_ERR_PENDING;
}

dns_status_t dns_input(dns_resolver_t *res, const void *msg, uint32_t len)
{
    const uint8_t *m = (const uint8_t *)msg;
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t rcode;
    uint32_t off;
    uint32_t i;

    if (res == NULL || msg == NULL) {
        return DNS_ERR_PARAM;
    }
    if (!res->initialized) {
        return DNS_ERR_NOT_READY;
    }
    if (len < DNS_HEADER_LEN) {
        return DNS_ERR_BAD_PACKET;
    }

    id      = dns_rd16(m);
    flags   = dns_rd16(m + 2u);
    qdcount = dns_rd16(m + 4u);
    ancount = dns_rd16(m + 6u);

    if (id != res->pending_id) {
        return DNS_ERR_BAD_PACKET;      /* not our transaction */
    }
    if ((flags & 0x8000u) == 0u) {      /* QR bit must be set */
        return DNS_ERR_BAD_PACKET;
    }
    if ((flags & 0x0200u) != 0u) {      /* TC */
        return DNS_ERR_TRUNCATED;
    }
    rcode = (uint16_t)(flags & 0x000Fu);
    if (rcode == 3u) {
        return DNS_ERR_NXDOMAIN;
    }
    if (rcode != 0u) {
        return DNS_ERR_RCODE;
    }

    off = DNS_HEADER_LEN;
    for (i = 0u; i < qdcount; ++i) {
        if (!dns_skip_name(m, len, &off)) {
            return DNS_ERR_BAD_PACKET;
        }
        if ((off + 4u) > len) {
            return DNS_ERR_BAD_PACKET;
        }
        off += 4u;                       /* QTYPE + QCLASS */
    }

    for (i = 0u; i < ancount; ++i) {
        uint16_t type;
        uint16_t cls;
        uint16_t rdlen;
        uint32_t ttl;

        if (!dns_skip_name(m, len, &off)) {
            return DNS_ERR_BAD_PACKET;
        }
        if ((off + 10u) > len) {
            return DNS_ERR_BAD_PACKET;
        }
        type  = dns_rd16(m + off);
        cls   = dns_rd16(m + off + 2u);
        ttl   = dns_rd32(m + off + 4u);
        rdlen = dns_rd16(m + off + 8u);
        off += 10u;
        if ((off + rdlen) > len) {
            return DNS_ERR_BAD_PACKET;
        }

        if (type == DNS_TYPE_A && cls == DNS_CLASS_IN && rdlen == 4u) {
            uint32_t ip = dns_get_ip(m + off);
            dns_cache_insert(res, res->pending_name, ip, ttl);
            return DNS_OK;
        }
        off += rdlen;
    }

    return DNS_ERR_NO_DATA;
}
