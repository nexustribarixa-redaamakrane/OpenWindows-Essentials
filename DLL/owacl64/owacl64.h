/*
 * owacl64.h - OpenWindows Access Control List Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWACL64_H
#define OWACL64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owacl_init(void);
void owacl_check(void);
void owacl_grant(void);
void owacl_revoke(void);
void owacl_enumerate(void);

#endif /* OWACL64_H */

