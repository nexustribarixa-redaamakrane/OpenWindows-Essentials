/*
 * pfwall.c - OpenWindows Packet Filter Firewall (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "pfwall.h"

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

/* A zero mask matches every address. */
static bool pfw_ip_match(uint32_t addr, uint32_t rule_ip, uint32_t mask)
{
    if (mask == 0u) {
        return true;
    }
    return (addr & mask) == (rule_ip & mask);
}

/* Both bounds zero means "any port"; an open upper bound means a
 * single port. */
static bool pfw_port_match(uint16_t port, uint16_t lo, uint16_t hi)
{
    if (lo == PFW_PORT_ANY && hi == PFW_PORT_ANY) {
        return true;
    }
    if (hi == PFW_PORT_ANY) {
        hi = lo;
    }
    return (port >= lo) && (port <= hi);
}

static bool pfw_rule_matches(const pfw_rule_t *r, const pfw_packet_t *p)
{
    if (!r->enabled) {
        return false;
    }
    if (r->proto != (uint8_t)PFW_PROTO_ANY && r->proto != p->proto) {
        return false;
    }
    if (r->direction != (uint8_t)PFW_DIR_ANY &&
        r->direction != p->direction) {
        return false;
    }
    if (!pfw_ip_match(p->src_ip, r->src_ip, r->src_mask)) {
        return false;
    }
    if (!pfw_ip_match(p->dst_ip, r->dst_ip, r->dst_mask)) {
        return false;
    }
    if (!pfw_port_match(p->src_port, r->src_port_min, r->src_port_max)) {
        return false;
    }
    if (!pfw_port_match(p->dst_port, r->dst_port_min, r->dst_port_max)) {
        return false;
    }
    return true;
}

static bool pfw_action_valid(pfw_action_t action)
{
    return action == PFW_ACTION_ALLOW || action == PFW_ACTION_DENY;
}

static bool pfw_proto_valid(uint8_t proto)
{
    return proto == (uint8_t)PFW_PROTO_ANY ||
           proto == (uint8_t)PFW_PROTO_ICMP ||
           proto == (uint8_t)PFW_PROTO_TCP ||
           proto == (uint8_t)PFW_PROTO_UDP;
}

static bool pfw_direction_valid(uint8_t dir)
{
    return dir == (uint8_t)PFW_DIR_IN ||
           dir == (uint8_t)PFW_DIR_OUT ||
           dir == (uint8_t)PFW_DIR_ANY;
}

/* ── Public API ───────────────────────────────────────────────── */

pfw_status_t pfw_init(pfw_table_t *fw, pfw_action_t default_action)
{
    if (fw == NULL || !pfw_action_valid(default_action)) {
        return PFW_ERR_PARAM;
    }
    ow_memset(fw, 0, sizeof(*fw));
    fw->default_action = default_action;
    fw->next_id        = 1u;
    fw->rule_count     = 0u;
    fw->initialized    = true;
    return PFW_OK;
}

pfw_status_t pfw_rule_init(pfw_rule_t *rule, pfw_action_t action)
{
    if (rule == NULL || !pfw_action_valid(action)) {
        return PFW_ERR_PARAM;
    }
    ow_memset(rule, 0, sizeof(*rule));
    rule->action    = action;
    rule->proto     = (uint8_t)PFW_PROTO_ANY;
    rule->direction = (uint8_t)PFW_DIR_ANY;
    rule->enabled   = 1u;
    return PFW_OK;
}

int32_t pfw_add_rule(pfw_table_t *fw, const pfw_rule_t *rule)
{
    pfw_rule_t *slot;

    if (fw == NULL || rule == NULL) {
        return -1;
    }
    if (!fw->initialized) {
        return -1;
    }
    if (!pfw_action_valid(rule->action) || !pfw_proto_valid(rule->proto) ||
        !pfw_direction_valid(rule->direction)) {
        return -1;
    }
    if (rule->src_port_min > rule->src_port_max &&
        rule->src_port_max != PFW_PORT_ANY) {
        return -1;
    }
    if (rule->dst_port_min > rule->dst_port_max &&
        rule->dst_port_max != PFW_PORT_ANY) {
        return -1;
    }
    if (fw->rule_count >= PFW_MAX_RULES) {
        return -1;
    }

    slot = &fw->rules[fw->rule_count];
    ow_memcpy(slot, rule, sizeof(*slot));
    slot->id      = fw->next_id;
    slot->enabled = (rule->enabled != 0u) ? 1u : 0u;
    slot->packets = 0u;
    fw->next_id++;
    fw->rule_count++;
    return (int32_t)slot->id;
}

pfw_status_t pfw_remove_rule(pfw_table_t *fw, uint32_t rule_id)
{
    uint32_t i;

    if (fw == NULL) {
        return PFW_ERR_PARAM;
    }
    if (!fw->initialized) {
        return PFW_ERR_NOT_READY;
    }

    for (i = 0u; i < fw->rule_count; ++i) {
        if (fw->rules[i].id == rule_id) {
            /* Compact to preserve first-match ordering. */
            uint32_t j;
            for (j = (i + 1u); j < fw->rule_count; ++j) {
                ow_memcpy(&fw->rules[j - 1u], &fw->rules[j],
                          sizeof(pfw_rule_t));
            }
            fw->rule_count--;
            ow_memset(&fw->rules[fw->rule_count], 0,
                      sizeof(fw->rules[fw->rule_count]));
            return PFW_OK;
        }
    }
    return PFW_ERR_NOT_FOUND;
}

pfw_status_t pfw_flush_rules(pfw_table_t *fw)
{
    if (fw == NULL) {
        return PFW_ERR_PARAM;
    }
    if (!fw->initialized) {
        return PFW_ERR_NOT_READY;
    }
    ow_memset(fw->rules, 0, sizeof(fw->rules));
    fw->rule_count = 0u;
    fw->next_id    = 1u;
    return PFW_OK;
}

pfw_action_t pfw_filter_packet(pfw_table_t *fw, const pfw_packet_t *pkt)
{
    uint32_t i;

    if (fw == NULL || pkt == NULL || !fw->initialized) {
        return PFW_ACTION_DENY;   /* fail closed */
    }

    for (i = 0u; i < fw->rule_count; ++i) {
        if (pfw_rule_matches(&fw->rules[i], pkt)) {
            fw->rules[i].packets++;
            return fw->rules[i].action;   /* first match wins */
        }
    }
    return fw->default_action;
}
