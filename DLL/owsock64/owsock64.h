/*
 * owsock64.h - OpenWindows Socket Abstraction Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSOCK64_H
#define OWSOCK64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owsock_create(void);
void owsock_bind(void);
void owsock_listen(void);
void owsock_accept(void);
void owsock_connect(void);
void owsock_send(void);
void owsock_recv(void);
void owsock_close(void);

#endif /* OWSOCK64_H */

