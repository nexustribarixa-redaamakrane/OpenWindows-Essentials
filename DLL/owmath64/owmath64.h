/*
 * owmath64.h - OpenWindows Fixed-Point Math Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWMATH64_H
#define OWMATH64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owmath_abs(void);
void owmath_min(void);
void owmath_max(void);
void owmath_clamp(void);
void owmath_sqrt_approx(void);
void owmath_div_round(void);

#endif /* OWMATH64_H */

