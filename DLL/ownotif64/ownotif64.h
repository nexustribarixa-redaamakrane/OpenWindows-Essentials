/*
 * ownotif64.h - OpenWindows Notification Service Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWNOTIF64_H
#define OWNOTIF64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void ownotif_init(void);
void ownotif_send(void);
void ownotif_dismiss(void);
void ownotif_register(void);
void ownotif_get_pending(void);

#endif /* OWNOTIF64_H */

