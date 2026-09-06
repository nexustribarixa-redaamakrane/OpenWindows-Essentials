/*
 * kbdlayout.h - OpenWindows Keyboard Layout Manager (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef KBDLAYOUT_H
#define KBDLAYOUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void kbl_init(void);
void kbl_set_layout(void);
void kbl_scancode_to_char(void);
void kbl_get_current(void);

#endif /* KBDLAYOUT_H */

