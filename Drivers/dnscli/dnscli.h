/*
 * dnscli.h - OpenWindows DNS Resolver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef DNSCLI_H
#define DNSCLI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void dns_init(void);
void dns_resolve(void);
void dns_cache_flush(void);
void dns_set_server(void);

#endif /* DNSCLI_H */

