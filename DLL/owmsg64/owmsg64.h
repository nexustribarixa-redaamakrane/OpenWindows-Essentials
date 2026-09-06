/*
 * owmsg64.h - OpenWindows Message Queue Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWMSG64_H
#define OWMSG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owmsg_init(void);
void owmsg_send(void);
void owmsg_recv(void);
void owmsg_peek(void);
void owmsg_flush(void);

#endif /* OWMSG64_H */

