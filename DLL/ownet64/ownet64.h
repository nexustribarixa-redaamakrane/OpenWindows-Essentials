/*
 * ownet64.h - OpenWindows Network Stack Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWNET64_H
#define OWNET64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void ownet_init(void);
void ownet_socket(void);
void ownet_bind(void);
void ownet_connect(void);
void ownet_send(void);
void ownet_recv(void);
void ownet_close(void);

#endif /* OWNET64_H */

