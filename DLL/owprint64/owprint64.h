/*
 * owprint64.h - OpenWindows Print Spooler Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPRINT64_H
#define OWPRINT64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owprint_init(void);
void owprint_submit(void);
void owprint_cancel(void);
void owprint_get_queue(void);
void owprint_enum_printers(void);

#endif /* OWPRINT64_H */

