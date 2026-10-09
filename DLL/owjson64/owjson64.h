/*
 * owjson64.h - OpenWindows Minimal JSON Parser (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWJSON64_H
#define OWJSON64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owjson_init(void);
void owjson_parse(void);
void owjson_get_string(void);
void owjson_get_number(void);
void owjson_get_array(void);
void owjson_free(void);

#endif /* OWJSON64_H */

