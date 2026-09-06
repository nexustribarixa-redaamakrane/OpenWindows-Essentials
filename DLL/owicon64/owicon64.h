/*
 * owicon64.h - OpenWindows Icon/Bitmap Resource Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWICON64_H
#define OWICON64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owicon_init(void);
void owicon_load(void);
void owicon_draw(void);
void owicon_get_size(void);
void owicon_create(void);

#endif /* OWICON64_H */

