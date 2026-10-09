/*
 * unitrans64.h - OpenWindows Unicode Translation Dynamic Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef UNITRANS64_H
#define UNITRANS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

uint32_t unitrans64_to_sucs(const uint8_t *utf8, size_t len, uint32_t *out_sucs, size_t max, size_t *out_len);
uint32_t unitrans64_to_sutf8(const uint32_t *sucs, size_t count, uint8_t *out_utf8, size_t max, size_t *out_len);
bool unitrans64_validate_sucs(uint32_t cp);

#endif /* UNITRANS64_H */
