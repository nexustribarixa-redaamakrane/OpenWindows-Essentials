/*
 * owmenu64.h - OpenWindows Menu System Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWMENU64_H
#define OWMENU64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owmenu_init(void);
void owmenu_create(void);
void owmenu_add_item(void);
void owmenu_show(void);
void owmenu_destroy(void);
void owmenu_get_selection(void);

#endif /* OWMENU64_H */

