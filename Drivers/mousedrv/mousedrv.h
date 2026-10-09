/*
 * mousedrv.h - OpenWindows Mouse Input Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef MOUSEDRV_H
#define MOUSEDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void mouse_init(void);
void mouse_poll(void);
void mouse_get_state(void);
void mouse_set_rate(void);

#endif /* MOUSEDRV_H */

