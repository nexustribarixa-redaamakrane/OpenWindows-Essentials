/*
 * loopback.h - OpenWindows Loopback Block Device (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef LOOPBACK_H
#define LOOPBACK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void loop_init(void);
void loop_attach(void);
void loop_detach(void);
void loop_read(void);
void loop_write(void);

#endif /* LOOPBACK_H */

