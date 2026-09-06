/*
 * owtls64.h - OpenWindows TLS/Crypto Transport Layer (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWTLS64_H
#define OWTLS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owtls_init(void);
void owtls_handshake(void);
void owtls_send(void);
void owtls_recv(void);
void owtls_close(void);

#endif /* OWTLS64_H */

