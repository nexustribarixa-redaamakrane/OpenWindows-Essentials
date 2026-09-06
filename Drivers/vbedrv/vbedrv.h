/*
 * vbedrv.h - OpenWindows VESA BIOS Extensions Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef VBEDRV_H
#define VBEDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void vbe_init(void);
void vbe_set_mode(void);
void vbe_get_framebuffer(void);
void vbe_get_info(void);

#endif /* VBEDRV_H */

