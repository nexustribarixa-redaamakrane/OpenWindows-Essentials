/*
 * owbitmap64.h - OpenWindows Bitmap Allocator (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWBITMAP64_H
#define OWBITMAP64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owbmp_init(void);
void owbmp_alloc(void);
void owbmp_free(void);
void owbmp_test(void);
void owbmp_find_first_free(void);

#endif /* OWBITMAP64_H */

