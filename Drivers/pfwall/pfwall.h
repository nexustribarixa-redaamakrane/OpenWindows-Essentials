/*
 * pfwall.h - OpenWindows Packet Filter Firewall (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PFWALL_H
#define PFWALL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void pfw_init(void);
void pfw_add_rule(void);
void pfw_remove_rule(void);
void pfw_filter_packet(void);
void pfw_flush_rules(void);

#endif /* PFWALL_H */

