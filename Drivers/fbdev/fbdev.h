/*
 * fbdev.h - OpenWindows Framebuffer Device Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef FBDEV_H
#define FBDEV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void fbdev_init(void);
void fbdev_putpixel(void);
void fbdev_fill_rect(void);
void fbdev_blit(void);
void fbdev_clear(void);

#endif /* FBDEV_H */

