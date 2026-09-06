/*
 * i8042.h - OpenWindows i8042 PS/2 Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef I8042_H
#define I8042_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void i8042_init(void);
void i8042_send_cmd(void);
void i8042_read_data(void);
void i8042_flush(void);

#endif /* I8042_H */

