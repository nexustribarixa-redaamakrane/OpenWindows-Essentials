/*
 * owevent64.h - OpenWindows Event Bus Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWEVENT64_H
#define OWEVENT64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owevent_init(void);
void owevent_subscribe(void);
void owevent_unsubscribe(void);
void owevent_publish(void);
void owevent_poll(void);

#endif /* OWEVENT64_H */

