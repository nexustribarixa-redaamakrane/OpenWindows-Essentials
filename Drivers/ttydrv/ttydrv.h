/*
 * ttydrv.h - OpenWindows TTY Console Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef TTYDRV_H
#define TTYDRV_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void tty_init(void);
void tty_write_char(void);
void tty_write_string(void);
void tty_clear(void);
void tty_scroll(void);

#endif /* TTYDRV_H */

