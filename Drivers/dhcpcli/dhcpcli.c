/*
 * dhcpcli.c - OpenWindows DHCP Client (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "dhcpcli.h"

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

static inline uint16_t ow_bswap16(uint16_t v)
{
    return (uint16_t)((uint16_t)(v << 8) | (uint16_t)(v >> 8));
}

static inline uint32_t ow_bswap32(uint32_t v)
{
    return ((v & 0x000000FFu) << 24) | ((v & 0x0000FF00u) << 8) |
           ((v & 0x00FF0000u) >> 8)  | ((v & 0xFF000000u) >> 24);
}

/* IP fields are copied byte-for-byte so they share the IP stack's
 * network-order representation on any host endianness. */
static void dhcp_put_ip(uint8_t *dst, uint32_t ip)
{
    ow_memcpy(dst, &ip, 4u);
}

static uint32_t dhcp_get_ip(const uint8_t *src)
{
    uint32_t ip;
    ow_memcpy(&ip, src, 4u);
    return ip;
}

/* ── Transaction identifier generator ─────────────────────────── */

static uint32_t dhcp_next_xid(dhcp_client_t *cli)
{
    cli->seed = (cli->seed * 1103515245u) + 12345u;
    return (cli->seed >> 8) ^ (uint32_t)cli->mac[5];
}

/* ── Message construction ─────────────────────────────────────── */

static uint32_t dhcp_build(dhcp_client_t *cli, uint8_t msg_type,
                           uint32_t requested_ip, uint32_t server_id)
{
    dhcp_message_t *m = (dhcp_message_t *)(void *)cli->scratch;
    uint32_t opt = 0u;
    uint32_t base;
    uint32_t total;
    uint32_t i;

    ow_memset(cli->scratch, 0, sizeof(cli->scratch));

    m->op    = 1u;                 /* BOOTREQUEST */
    m->htype = 1u;                 /* Ethernet    */
    m->hlen  = 6u;
    m->hops  = 0u;
    m->xid   = ow_bswap32(cli->xid);
    m->secs  = 0u;
    m->flags = ow_bswap16(DHCP_FLAG_BROADCAST);
    m->ciaddr = (msg_type == DHCPRELEASE) ? cli->client_ip : 0u;
    m->yiaddr = 0u;
    m->siaddr = 0u;
    m->giaddr = 0u;
    for (i = 0u; i < 6u; ++i) {
        m->chaddr[i] = cli->mac[i];
    }
    m->magic = ow_bswap32(DHCP_MAGIC_COOKIE);

    m->options[opt++] = DHCP_OPT_MSG_TYPE;
    m->options[opt++] = 1u;
    m->options[opt++] = msg_type;

    if (requested_ip != DHCP_IP_ANY) {
        m->options[opt++] = DHCP_OPT_REQUESTED;
        m->options[opt++] = 4u;
        dhcp_put_ip(&m->options[opt], requested_ip);
        opt += 4u;
    }
    if (server_id != DHCP_IP_ANY) {
        m->options[opt++] = DHCP_OPT_SERVER_ID;
        m->options[opt++] = 4u;
        dhcp_put_ip(&m->options[opt], server_id);
        opt += 4u;
    }
    if (msg_type != DHCPRELEASE) {
        m->options[opt++] = DHCP_OPT_PARAM_LIST;
        m->options[opt++] = 3u;
        m->options[opt++] = DHCP_OPT_SUBNET;
        m->options[opt++] = DHCP_OPT_ROUTER;
        m->options[opt++] = DHCP_OPT_DNS;
    }
    m->options[opt++] = DHCP_OPT_END;

    /* Offset of the options field inside the fixed BOOTP header. */
    base = (uint32_t)((const uint8_t *)&m->options - (const uint8_t *)m);
    total = base + opt;
    if (total < DHCP_MIN_MESSAGE) {
        total = DHCP_MIN_MESSAGE;
    }
    return total;
}

/* ── Reply parsing ────────────────────────────────────────────── */

static uint8_t dhcp_parse_options(dhcp_client_t *cli, const dhcp_message_t *m,
                                  bool *has_server_id)
{
    uint32_t i = 0u;
    uint8_t  type = 0u;

    *has_server_id = false;
    while (i < sizeof(m->options)) {
        uint8_t code = m->options[i++];
        uint8_t olen;

        if (code == DHCP_OPT_PAD) {
            continue;
        }
        if (code == DHCP_OPT_END) {
            break;
        }
        if (i >= sizeof(m->options)) {
            break;
        }
        olen = m->options[i++];
        if ((i + olen) > sizeof(m->options)) {
            break;
        }

        switch (code) {
        case DHCP_OPT_MSG_TYPE:
            if (olen >= 1u) {
                type = m->options[i];
            }
            break;
        case DHCP_OPT_SUBNET:
            if (olen >= 4u) {
                cli->netmask = dhcp_get_ip(&m->options[i]);
            }
            break;
        case DHCP_OPT_ROUTER:
            if (olen >= 4u) {
                cli->router = dhcp_get_ip(&m->options[i]);
            }
            break;
        case DHCP_OPT_DNS:
            if (olen >= 4u) {
                cli->dns = dhcp_get_ip(&m->options[i]);
            }
            break;
        case DHCP_OPT_LEASE_TIME:
            if (olen >= 4u) {
                cli->lease_seconds = ow_bswap32(dhcp_get_ip(&m->options[i]));
            }
            break;
        case DHCP_OPT_SERVER_ID:
            if (olen >= 4u) {
                cli->server_ip = dhcp_get_ip(&m->options[i]);
                *has_server_id = true;
            }
            break;
        default:
            break;
        }
        i += olen;
    }
    return type;
}

/* ── Public API ───────────────────────────────────────────────── */

dhcp_status_t dhcp_init(dhcp_client_t *cli, const uint8_t mac[6],
                        dhcp_udp_tx_fn tx, void *arg)
{
    uint32_t i;

    if (cli == NULL || mac == NULL || tx == NULL) {
        return DHCP_ERR_PARAM;
    }
    ow_memset(cli, 0, sizeof(*cli));
    for (i = 0u; i < 6u; ++i) {
        cli->mac[i] = mac[i];
    }
    cli->udp_tx  = tx;
    cli->udp_arg = arg;
    cli->seed    = 0x2545F491u ^ (uint32_t)mac[3];
    cli->state   = DHCP_STATE_IDLE;
    cli->initialized = true;
    return DHCP_OK;
}

dhcp_status_t dhcp_discover(dhcp_client_t *cli)
{
    uint32_t len;

    if (cli == NULL) {
        return DHCP_ERR_PARAM;
    }
    if (!cli->initialized) {
        return DHCP_ERR_NOT_READY;
    }

    cli->xid        = dhcp_next_xid(cli);
    cli->offered_ip = DHCP_IP_ANY;
    cli->server_ip  = DHCP_IP_ANY;
    len = dhcp_build(cli, DHCPDISCOVER, DHCP_IP_ANY, DHCP_IP_ANY);
    cli->state = DHCP_STATE_SELECTING;

    return cli->udp_tx(cli->udp_arg, DHCP_IP_ANY, DHCP_CLIENT_PORT,
                       DHCP_IP_BROADCAST, DHCP_SERVER_PORT,
                       cli->scratch, len);
}

dhcp_status_t dhcp_request(dhcp_client_t *cli)
{
    uint32_t len;

    if (cli == NULL) {
        return DHCP_ERR_PARAM;
    }
    if (!cli->initialized) {
        return DHCP_ERR_NOT_READY;
    }
    if (cli->state != DHCP_STATE_SELECTING ||
        cli->offered_ip == DHCP_IP_ANY || cli->server_ip == DHCP_IP_ANY) {
        return DHCP_ERR_STATE;   /* no OFFER to request */
    }

    len = dhcp_build(cli, DHCPREQUEST, cli->offered_ip, cli->server_ip);
    cli->state = DHCP_STATE_REQUESTING;

    return cli->udp_tx(cli->udp_arg, DHCP_IP_ANY, DHCP_CLIENT_PORT,
                       DHCP_IP_BROADCAST, DHCP_SERVER_PORT,
                       cli->scratch, len);
}

dhcp_status_t dhcp_release(dhcp_client_t *cli)
{
    uint32_t len;
    dhcp_status_t rc;

    if (cli == NULL) {
        return DHCP_ERR_PARAM;
    }
    if (!cli->initialized) {
        return DHCP_ERR_NOT_READY;
    }
    if (cli->state != DHCP_STATE_BOUND && cli->state != DHCP_STATE_RENEWING) {
        return DHCP_ERR_STATE;
    }

    len = dhcp_build(cli, DHCPRELEASE, DHCP_IP_ANY, cli->server_ip);
    cli->state = DHCP_STATE_RELEASING;

    rc = cli->udp_tx(cli->udp_arg, cli->client_ip, DHCP_CLIENT_PORT,
                     cli->server_ip, DHCP_SERVER_PORT, cli->scratch, len);

    cli->state          = DHCP_STATE_IDLE;
    cli->client_ip      = DHCP_IP_ANY;
    cli->offered_ip     = DHCP_IP_ANY;
    cli->server_ip      = DHCP_IP_ANY;
    cli->netmask        = DHCP_IP_ANY;
    cli->router         = DHCP_IP_ANY;
    cli->dns            = DHCP_IP_ANY;
    cli->lease_seconds  = 0u;
    cli->lease_remaining = 0u;
    return rc;
}

dhcp_status_t dhcp_input(dhcp_client_t *cli, const void *packet,
                         uint32_t len)
{
    const dhcp_message_t *m;
    bool has_server_id;
    uint8_t type;
    uint32_t xid;

    if (cli == NULL || packet == NULL) {
        return DHCP_ERR_PARAM;
    }
    if (!cli->initialized) {
        return DHCP_ERR_NOT_READY;
    }
    if (len < DHCP_MIN_MESSAGE) {
        return DHCP_ERR_BAD_PACKET;
    }

    m = (const dhcp_message_t *)packet;
    if (m->op != 2u) {                      /* must be BOOTREPLY */
        return DHCP_ERR_BAD_PACKET;
    }
    if (ow_bswap32(m->magic) != DHCP_MAGIC_COOKIE) {
        return DHCP_ERR_BAD_PACKET;
    }
    xid = ow_bswap32(m->xid);
    if (xid != cli->xid) {
        return DHCP_ERR_BAD_PACKET;         /* not our transaction */
    }
    if (m->chaddr[0] != cli->mac[0] || m->chaddr[1] != cli->mac[1] ||
        m->chaddr[2] != cli->mac[2]) {
        return DHCP_ERR_BAD_PACKET;
    }

    type = dhcp_parse_options(cli, m, &has_server_id);

    switch (type) {
    case DHCPOFFER:
        if (cli->state != DHCP_STATE_SELECTING) {
            return DHCP_ERR_STATE;
        }
        cli->offered_ip = m->yiaddr;
        if (!has_server_id) {
            cli->server_ip = m->siaddr;
        }
        return DHCP_OK;

    case DHCPACK:
        if (cli->state != DHCP_STATE_REQUESTING &&
            cli->state != DHCP_STATE_RENEWING) {
            return DHCP_ERR_STATE;
        }
        cli->client_ip = m->yiaddr;
        if (cli->lease_seconds == 0u) {
            cli->lease_seconds = 3600u;     /* sane fallback */
        }
        cli->lease_remaining = cli->lease_seconds;
        cli->state = DHCP_STATE_BOUND;
        return DHCP_OK;

    case DHCPNAK:
        cli->client_ip      = DHCP_IP_ANY;
        cli->offered_ip     = DHCP_IP_ANY;
        cli->server_ip      = DHCP_IP_ANY;
        cli->netmask        = DHCP_IP_ANY;
        cli->router         = DHCP_IP_ANY;
        cli->dns            = DHCP_IP_ANY;
        cli->lease_seconds  = 0u;
        cli->lease_remaining = 0u;
        cli->state = DHCP_STATE_IDLE;
        return DHCP_ERR_NAK;

    default:
        return DHCP_ERR_BAD_PACKET;
    }
}

dhcp_status_t dhcp_tick(dhcp_client_t *cli, uint32_t elapsed_seconds)
{
    if (cli == NULL) {
        return DHCP_ERR_PARAM;
    }
    if (!cli->initialized) {
        return DHCP_ERR_NOT_READY;
    }
    if (cli->state != DHCP_STATE_BOUND && cli->state != DHCP_STATE_RENEWING) {
        return DHCP_OK;
    }

    if (cli->lease_remaining <= elapsed_seconds) {
        /* Lease expired: drop the address and fall back to DISCOVER. */
        cli->lease_remaining = 0u;
        cli->client_ip       = DHCP_IP_ANY;
        cli->netmask         = DHCP_IP_ANY;
        cli->router          = DHCP_IP_ANY;
        cli->dns             = DHCP_IP_ANY;
        cli->state           = DHCP_STATE_IDLE;
    } else {
        cli->lease_remaining -= elapsed_seconds;
    }
    return DHCP_OK;
}

dhcp_state_t dhcp_state(const dhcp_client_t *cli)
{
    if (cli == NULL) {
        return DHCP_STATE_IDLE;
    }
    return cli->state;
}
