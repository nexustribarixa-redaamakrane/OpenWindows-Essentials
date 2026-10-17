/*
 * owmath64.c - OpenWindows Fixed-Point Math Library (.owd)
 *
 * Branch-light integer helpers plus an exact integer square root. No
 * floating point and no dynamic allocation.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owmath64.h"

/* ── Implementation ──────────────────────────────────────────── */

int64_t owmath_abs(int64_t v)
{
    if (v >= 0) return v;
    /* -INT64_MIN cannot be represented in two's complement. */
    if (v == INT64_MIN) return INT64_MAX;
    return -v;
}

int64_t owmath_min(int64_t a, int64_t b)
{
    return (a < b) ? a : b;
}

int64_t owmath_max(int64_t a, int64_t b)
{
    return (a > b) ? a : b;
}

int64_t owmath_clamp(int64_t v, int64_t lo, int64_t hi)
{
    if (lo > hi) return lo;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

uint64_t owmath_sqrt_approx(uint64_t value)
{
    if (value == 0u) return 0u;

    /* Bit-by-bit integer square root: start from the highest power of
     * four that fits in 64 bits and resolve two result bits per step. */
    uint64_t result = 0u;
    uint64_t bit = (uint64_t)1u << 62;
    while (bit > value) bit >>= 2;

    uint64_t remainder = value;
    while (bit != 0u) {
        if (remainder >= result + bit) {
            remainder -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

int64_t owmath_div_round(int64_t num, int64_t den)
{
    if (den == 0) return 0;

    bool negative = ((num < 0) != (den < 0));

    /* Magnitudes as uint64_t so INT64_MIN is handled without overflow. */
    uint64_t a = (num < 0) ? ((uint64_t)(-(num + 1)) + 1u) : (uint64_t)num;
    uint64_t b = (den < 0) ? ((uint64_t)(-(den + 1)) + 1u) : (uint64_t)den;

    uint64_t quotient = a / b;
    uint64_t remainder = a % b;

    /* Round half away from zero; r >= b - r is 2*r >= b without overflow. */
    if (remainder != 0u && remainder >= (b - remainder)) {
        quotient += 1u;
    }

    if (!negative) {
        if (quotient > (uint64_t)INT64_MAX) return INT64_MAX;
        return (int64_t)quotient;
    }

    /* Negative range extends one further than the positive range. */
    if (quotient > ((uint64_t)INT64_MAX + 1u)) return INT64_MIN;
    if (quotient == ((uint64_t)INT64_MAX + 1u)) return INT64_MIN;
    return -(int64_t)quotient;
}
