/*
 * pfwall.h - OpenWindows Packet Filter Firewall (.owc)
 *
 * Stateless first-match packet filter over a fixed, ordered rule table
 * with a configurable default action. Rules match on protocol,
 * direction, masked source/destination addresses and port ranges.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PFWALL_H
#define PFWALL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Status codes ─────────────────────────────────────────────── */

typedef uint32_t pfw_status_t;

#define PFW_OK            0x00000000u  /* Completed                    */
#define PFW_ERR_PARAM     0x00000001u  /* Invalid argument             */
#define PFW_ERR_NOT_READY 0x00000002u  /* Firewall not initialised     */
#define PFW_ERR_FULL      0x00000003u  /* Rule table full              */
#define PFW_ERR_NOT_FOUND 0x00000004u  /* No rule with that id         */

/* ── Rule classification ──────────────────────────────────────── */

typedef enum {
    PFW_ACTION_DENY = 0,
    PFW_ACTION_ALLOW = 1
} pfw_action_t;

typedef enum {
    PFW_PROTO_ANY = 0,
    PFW_PROTO_ICMP = 1,
    PFW_PROTO_TCP = 6,
    PFW_PROTO_UDP = 17
} pfw_proto_t;

typedef enum {
    PFW_DIR_IN = 0,
    PFW_DIR_OUT = 1,
    PFW_DIR_ANY = 2
} pfw_direction_t;

/* ── Capacity limits (static storage) ─────────────────────────── */

#define PFW_MAX_RULES 32u
#define PFW_PORT_ANY  0u       /* Both bounds zero => any port         */

/* ── Rule and packet descriptions ─────────────────────────────── */

typedef struct {
    uint32_t       id;             /* Assigned by add_rule            */
    pfw_action_t   action;
    uint8_t        proto;          /* pfw_proto_t value               */
    uint8_t        direction;      /* pfw_direction_t value           */
    uint8_t        enabled;
    uint8_t        reserved;
    uint32_t       src_ip;         /* Network byte order              */
    uint32_t       src_mask;       /* 0 = wildcard                    */
    uint32_t       dst_ip;
    uint32_t       dst_mask;       /* 0 = wildcard                    */
    uint16_t       src_port_min;
    uint16_t       src_port_max;
    uint16_t       dst_port_min;
    uint16_t       dst_port_max;
    uint32_t       packets;        /* Match counter                   */
} pfw_rule_t;

typedef struct {
    uint8_t  proto;                /* pfw_proto_t value               */
    uint8_t  direction;            /* pfw_direction_t value           */
    uint16_t reserved;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
} pfw_packet_t;

typedef struct {
    bool         initialized;
    pfw_action_t default_action;
    uint32_t     next_id;
    uint32_t     rule_count;
    pfw_rule_t   rules[PFW_MAX_RULES];
} pfw_table_t;

/* ── API ──────────────────────────────────────────────────────── */

pfw_status_t pfw_init(pfw_table_t *fw, pfw_action_t default_action);
int32_t      pfw_add_rule(pfw_table_t *fw, const pfw_rule_t *rule);
pfw_status_t pfw_remove_rule(pfw_table_t *fw, uint32_t rule_id);
pfw_action_t pfw_filter_packet(pfw_table_t *fw, const pfw_packet_t *pkt);
pfw_status_t pfw_flush_rules(pfw_table_t *fw);

/* ── Rule construction helper ─────────────────────────────────── */

pfw_status_t pfw_rule_init(pfw_rule_t *rule, pfw_action_t action);

#endif /* PFWALL_H */
