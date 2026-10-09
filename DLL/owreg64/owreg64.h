/*
 * owreg64.h - OpenWindows Registry Access Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWREG64_H
#define OWREG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owreg_open(void);
void owreg_close(void);
void owreg_read(void);
void owreg_write(void);
void owreg_delete(void);
void owreg_enum_keys(void);

#endif /* OWREG64_H */

