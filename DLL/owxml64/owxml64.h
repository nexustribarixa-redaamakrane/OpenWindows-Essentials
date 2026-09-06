/*
 * owxml64.h - OpenWindows Minimal XML Parser (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWXML64_H
#define OWXML64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owxml_init(void);
void owxml_parse(void);
void owxml_get_root(void);
void owxml_find_element(void);
void owxml_get_attr(void);
void owxml_free(void);

#endif /* OWXML64_H */

