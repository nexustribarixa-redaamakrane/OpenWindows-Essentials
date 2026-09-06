/*
 * owdnd64.h - OpenWindows Drag and Drop Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDND64_H
#define OWDND64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owdnd_init(void);
void owdnd_begin_drag(void);
void owdnd_drop(void);
void owdnd_register_target(void);
void owdnd_get_data(void);

#endif /* OWDND64_H */

