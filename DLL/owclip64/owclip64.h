/*
 * owclip64.h - OpenWindows Clipboard Manager (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWCLIP64_H
#define OWCLIP64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owclip_init(void);
void owclip_copy(void);
void owclip_paste(void);
void owclip_clear(void);
void owclip_get_format(void);

#endif /* OWCLIP64_H */

