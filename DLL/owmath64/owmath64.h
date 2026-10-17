/*
 * owmath64.h - OpenWindows Fixed-Point Math Library (.owd)
 *
 * Integer and fixed-point primitives for the OpenWindows runtime. All
 * routines operate on plain 64-bit signed/unsigned integers; the
 * OWMATH_FIXED_* macros describe a Q32.32 interpretation of the same
 * owmath_int_t scalar.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWMATH64_H
#define OWMATH64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Scalar used by every owmath entry point. */
typedef int64_t owmath_int_t;

/* Q32.32 fixed-point helpers. */
#define OWMATH_FIXED_SHIFT 32
#define OWMATH_FIXED_ONE   ((owmath_int_t)1 << OWMATH_FIXED_SHIFT)

/* Absolute value of v; INT64_MIN saturates to INT64_MAX. */
int64_t owmath_abs(int64_t v);

/* Smaller of two values. */
int64_t owmath_min(int64_t a, int64_t b);

/* Larger of two values. */
int64_t owmath_max(int64_t a, int64_t b);

/* Clamp v into [lo, hi]; returns lo when lo > hi. */
int64_t owmath_clamp(int64_t v, int64_t lo, int64_t hi);

/* Integer square root: floor(sqrt(value)) for unsigned 64-bit input. */
uint64_t owmath_sqrt_approx(uint64_t value);

/* Division rounded to nearest, ties away from zero; 0 when den == 0. */
int64_t owmath_div_round(int64_t num, int64_t den);

#endif /* OWMATH64_H */
