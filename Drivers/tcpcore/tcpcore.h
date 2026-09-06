/*
 * tcpcore.h - OpenWindows TCP Transport Core (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef TCPCORE_H
#define TCPCORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void tcp_init(void);
void tcp_connect(void);
void tcp_listen(void);
void tcp_send(void);
void tcp_recv(void);
void tcp_close(void);

#endif /* TCPCORE_H */

