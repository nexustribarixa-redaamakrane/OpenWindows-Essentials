/*
 * ow_math.c — freestanding libm subset for the genuine Cairo/pixman port.
 * Compiled -nostdlib -fno-builtin. See ow_runtime.h.
 *
 * Accuracy target: ~1e-13 relative (far beyond visual/rendering need).
 * Algorithms: exponent arithmetic + rational/series kernels (Cody-Waite
 * style range reduction for exp/trig), Newton iteration for cbrt.
 */
#include "ow_runtime.h"

#define OW_PI        3.14159265358979323846
#define OW_PI2       1.57079632679489661923
#define OW_PI4       0.78539816339744830962
#define OW_LN2       0.69314718055994530942
#define OW_LN10      2.30258509299404568402
#define OW_SQRT2     1.41421356237309504880
#define OW_1_SQRT2   0.70710678118654752440

static uint64_t ow_dbits(double x) { uint64_t u; memcpy(&u, &x, sizeof u); return u; }
static double   ow_dval(uint64_t u) { double x; memcpy(&x, &u, sizeof x); return x; }

static int ow_dexp(double x) { return (int)((ow_dbits(x) >> 52) & 0x7FFu); }
static int ow_dsign(double x) { return (int)(ow_dbits(x) >> 63); }
static double ow_pow2i(int e)
{
    if (e >= 1023) return 1.0 / 0.0;
    if (e <= -1075) return 0.0;
    return ow_dval(UINT64_C(0x3FF0000000000000) + ((uint64_t)e << 52));
}

/* ---- bit / classify ------------------------------------------------- */

double fabs(double x) { return ow_dval(ow_dbits(x) & ~(UINT64_C(1) << 63)); }
double copysign(double x, double y)
{
    uint64_t u = ow_dbits(x);
    uint64_t v = ow_dbits(y);
    return ow_dval((u & ~(UINT64_C(1) << 63)) | (v & (UINT64_C(1) << 63)));
}

/* ---- rounding ------------------------------------------------------ */

double ceil(double x)
{
    double t = trunc(x);
    if (x != x)
        return x;
    if (x > 0.0 && t != x)
        return t + 1.0;
    return t;
}

double floor(double x)
{
    double t;
    if (x != x)
        return x;
    t = trunc(x);
    if (x < 0.0 && t != x)
        return t - 1.0;
    return t;
}

double trunc(double x)
{
    if (x != x)
        return x;
    if (ow_dexp(x) == 0x7FF)
        return x;
    if (fabs(x) >= 9.2233720368547758e18)   /* 2^63 */
        return x;
    return copysign((double)(long long)x, x);
}

double round(double x)
{
    double t = trunc(x);
    double f = fabs(x - t);
    if (f >= 0.5) {
        if (x >= 0.0)
            return t + 1.0;
        return t - 1.0;
    }
    return t;
}

double rint(double x)
{
    double t = trunc(x);
    double d = fabs(x - t);
    if (d > 0.5)
        return x >= 0.0 ? t + 1.0 : t - 1.0;
    if (d == 0.5) {
        uint64_t u = ow_dbits(t);
        if ((u & 1u) != 0)
            return x >= 0.0 ? t + 1.0 : t - 1.0;
    }
    return t;
}

double nearbyint(double x) { return rint(x); }
long   lrint(double x) { return (long)rint(x); }
long long llrint(double x) { return (long long)rint(x); }

double fmin(double a, double b)
{
    if (a != a) return b;
    if (b != b) return a;
    if (a < b) return a;
    if (b < a) return b;
    return ow_dsign(a) ? a : b;   /* equal magnitudes -> preserve -0 */
}

double fmax(double a, double b)
{
    if (a != a) return b;
    if (b != b) return a;
    if (a > b) return a;
    if (b > a) return b;
    return ow_dsign(a) ? b : a;   /* equal magnitudes -> prefer +0 */
}

double fdim(double a, double b)
{
    return a > b ? a - b : 0.0;
}

/* we can't use ow_dbits != ow_dbits to detect NaN; fix above via sign+exp */
double fmod(double x, double y)
{
    double q;
    if (x != x || y != y)
        return 0.0 / 0.0;
    if (y == 0.0)
        return 0.0 / 0.0;
    if (ow_dexp(x) == 0x7FF)
        return 0.0 / 0.0;       /* inf -> NaN */
    if (ow_dexp(y) == 0x7FF)
        return x;               /* finite % inf == x */
    q = trunc(x / y);
    return x - q * y;
}

double remainder(double x, double y)
{
    double q = rint(x / y);
    return x - q * y;
}

double ldexp(double x, int e)
{
    if (x == 0.0 || ow_dexp(x) == 0x7FF)
        return x;
    return x * ow_pow2i(e);
}

double scalbn(double x, int e) { return ldexp(x, e); }

double frexp(double x, int *e)
{
    uint64_t u = ow_dbits(x);
    int ex = ow_dexp(x);
    if (ex == 0) {
        if (x == 0.0) { *e = 0; return x; }
        /* subnormal: scale up first */
        x = ldexp(x, 64);
        u = ow_dbits(x);
        ex = ow_dexp(x);
        ex -= 64;
    }
    if (ex == 0x7FF) {
        *e = 0;
        return x;
    }
    *e = ex - 1022;
    return ow_dval((u & (UINT64_C(1) << 63)) |
                   UINT64_C(0x3FE0000000000000));
}

double modf(double x, double *i)
{
    double ip = trunc(x);
    if (i) *i = ip;
    return x - ip;
}

/* ---- exp/log -------------------------------------------------------- */

double exp(double x)
{
    int k, i;
    double r, ln2hi, ln2lo, s, term, e2;

    if (x != x) return x;
    if (x > 709.78271289338397)
        return 1.0 / 0.0;
    if (x < -745.13321910194121)
        return 0.0;
    if (x == 0.0)
        return 1.0;

    k = (int)rint(x * 1.4426950408889634);
    ln2hi = 0.6931471805599453;
    ln2lo = 2.4468026987469666e-17;
    r = (x - (double)k * ln2hi) - (double)k * ln2lo;

    s = 1.0;
    term = 1.0;
    for (i = 1; i <= 26; ++i) {
        term *= r;
        term /= (double)i;
        s += term;
        if (term * term < 1e-40)
            break;
    }
    e2 = ow_pow2i(k);
    return s * e2;
}

double exp2(double x)
{
    return exp(x * OW_LN2);
}

double log(double x)
{
    double m, z, z2, s, term;
    int e;

    if (x != x) return x;
    if (x < 0.0)
        return 0.0 / 0.0;
    if (x == 0.0)
        return -1.0 / 0.0;
    if (ow_dexp(x) == 0x7FF)
        return x;

    m = frexp(x, &e);                  /* m in [0.5, 1) */
    if (m < OW_1_SQRT2) {
        m *= OW_SQRT2;
        --e;
    }                                   /* m in [0.707, 1.414), z small */
    z = (m - 1.0) / (m + 1.0);
    z2 = z * z;
    s = z;
    term = z;
    {
        int i;
        for (i = 1; i <= 40; ++i) {
            term *= z2;
            s += term / (double)(2 * i + 1);
            if (term * term < 1e-42)
                break;
        }
    }
    return 2.0 * s + (double)e * OW_LN2;
}

double log2(double x)  { return log(x) / OW_LN2; }
double log10(double x) { return log(x) / OW_LN10; }

double log1p(double x)
{
    double z, z2, s, term;
    int i;
    if (x <= -1.0)
        return x < -1.0 ? 0.0 / 0.0 : -1.0 / 0.0;
    if (x == 0.0)
        return x;
    z = x / (x + 2.0);
    z2 = z * z;
    s = z;
    term = z;
    for (i = 1; i <= 40; ++i) {
        term *= z2;
        s += term / (double)(2 * i + 1);
        if (term * term < 1e-42)
            break;
    }
    return 2.0 * s;
}

double pow(double x, double y)
{
    double a, p;
    int odd;
    if (y == 0.0)
        return 1.0;
    if (x == 1.0)
        return 1.0;
    if (x > 0.0)
        return exp(y * log(x));
    if (x == 0.0) {
        if (y > 0.0)
            return 0.0;
        return 1.0 / 0.0;          /* 0^neg inf */
    }
    if (x != x || y != y)
        return 0.0 / 0.0;
    if (x == -1.0) {
        p = rint(y);
        return (ow_dbits(p) & 1u) ? -1.0 : 1.0;
    }
    if (y != trunc(y) || fabs(y) > 9.0e15)
        return 0.0 / 0.0;          /* negative base, non-integer exp */
    a = exp(y * log(-x));
    odd = (int)(rint(y)) & 1;
    return odd ? -a : a;
}

/* ---- sqrt / cbrt ----------------------------------------------------- */

double sqrt(double x) { return __builtin_sqrt(x); }
float  sqrtf(float x) { return __builtin_sqrtf(x); }

double cbrt(double x)
{
    double a, y, dy, y2;
    int s, e;
    if (x == 0.0 || x != x)
        return x;
    if (ow_dexp(x) == 0x7FF)
        return x;
    s = (x < 0.0) ? -1 : 1;
    a = fabs(x);
    frexp(a, &e);
    y = ow_pow2i(e / 3);
    if (y < 1.0)
        y = 1.0;
    {
        int i;
        for (i = 0; i < 16; ++i) {
            y2 = y * y;
            dy = (a / y2 - y) / 3.0;   /* Newton: y <- (2y + a/y^2)/3 */
            y += dy;
            if (dy * dy < a * 1e-34 + 1e-34)
                break;
        }
    }
    return s * y;
}

/* ---- trig ------------------------------------------------------------ */

#define OW_TPI       6.28318530717958631786
#define OW_INV_TPI   0.15915494309189533577
#define OW_HPI       3.14159265358979323846
#define OW_INV_HPI   0.63661977236758134308
#define OW_HPI_HI    3.141592653589793115997964
#define OW_HPI_LO    1.2246467991473531772e-16

static void ow_sincos_kernel(double y, double *sn, double *cs)
{
    double r = y * y;
    *sn = y + y * r * (-0.16666666666666324348e0 +
                       r * (8.33333333332248946124e-3 +
                       r * (-1.98412698298579493134e-4 +
                       r * (2.75573137070700676789e-6 +
                       r * (-2.50507602534068634195e-8 +
                       r * 1.58969099521155010221e-10)))));
    *cs = 1.0 - 0.5 * r + r * r * (4.16666666666666019037e-2 +
                       r * (-1.38888888888741095749e-3 +
                       r * (2.48015872894767294178e-5 +
                       r * (-2.75573143513906633035e-7 +
                       r * (2.08757232129817482790e-9 +
                       r * (-1.13596475577881948265e-11))))));
}

double sin(double x)
{
    double xr, y, sn, cs;
    int qq, kq;

    if (x != x || ow_dexp(x) == 0x7FF)
        return 0.0 / 0.0;
    if (x == 0.0)
        return x;

    xr = x - rint(x * OW_INV_TPI) * OW_TPI;
    qq = (int)rint(xr * OW_INV_HPI);
    y = (xr - (double)qq * OW_HPI_HI) - (double)qq * OW_HPI_LO;

    kq = qq & 3;
    if (kq < 0)
        kq += 4;
    ow_sincos_kernel(y, &sn, &cs);
    switch (kq) {
    case 0: return sn;
    case 1: return cs;
    case 2: return -sn;
    default: return -cs;
    }
}

double cos(double x)
{
    double xr, y, sn, cs;
    int qq, kq;

    if (x != x || ow_dexp(x) == 0x7FF)
        return 0.0 / 0.0;
    if (x == 0.0)
        return 1.0;

    xr = x - rint(x * OW_INV_TPI) * OW_TPI;
    qq = (int)rint(xr * OW_INV_HPI);
    y = (xr - (double)qq * OW_HPI_HI) - (double)qq * OW_HPI_LO;

    kq = qq & 3;
    if (kq < 0)
        kq += 4;
    ow_sincos_kernel(y, &sn, &cs);
    switch (kq) {
    case 0: return cs;
    case 1: return -sn;
    case 2: return -cs;
    default: return sn;
    }
}

double tan(double x)
{
    double c = cos(x);
    if (c == 0.0)
        return 1.0 / 0.0;
    return sin(x) / c;
}

static double ow_atan_small(double b)
{
    double b2 = b * b, term = b, s = b, add;
    int i;
    for (i = 1; i < 40; ++i) {
        term *= -b2;
        add = term / (double)(2 * i + 1);
        s += add;
        if (add * add < 1e-36)
            break;
    }
    return s;
}

static double ow_atan_inner(double a)   /* a in [0,1] */
{
    int n = 0;
    while (a > 0.125 && n < 8) {
        a = a / (1.0 + sqrt(1.0 + a * a));
        ++n;
    }
    if (n == 0)
        return ow_atan_small(a);
    return ow_atan_small(a) * (double)(1 << n);
}

double atan(double x)
{
    double a;
    int s;
    if (x != x)
        return x;
    if (ow_dexp(x) == 0x7FF)
        return x > 0.0 ? OW_PI2 : -OW_PI2;
    if (x == 0.0)
        return x;
    s = (x < 0.0) ? -1 : 1;
    a = fabs(x);
    if (a > 1.0)
        return s * (OW_PI2 - ow_atan_inner(1.0 / a));
    return s * ow_atan_inner(a);
}

double asin(double x)
{
    if (x < -1.0 || x > 1.0)
        return 0.0 / 0.0;
    if (x == 1.0)  return OW_PI2;
    if (x == -1.0) return -OW_PI2;
    if (x == 0.0)
        return x;
    return atan(x / sqrt(1.0 - x * x));
}

double acos(double x)
{
    if (x < -1.0 || x > 1.0)
        return 0.0 / 0.0;
    if (x == 0.0)
        return OW_PI2;
    return OW_PI2 - asin(x);
}

double atan2(double y, double x)
{
    double a;
    if (x != x || y != y)
        return 0.0 / 0.0;
    if (x == 0.0) {
        if (y > 0.0) return OW_PI2;
        if (y < 0.0) return -OW_PI2;
        return ow_dsign(y) ? -0.0 : 0.0;
    }
    if (y == 0.0)
        return x > 0.0 ? (ow_dsign(y) ? -0.0 : 0.0) : (ow_dsign(y) ? -OW_PI : OW_PI);
    a = atan(y / x);
    if (x > 0.0)
        return a;
    return y > 0.0 ? a + OW_PI : a - OW_PI;
}

double hypot(double x, double y)
{
    double ax = fabs(x), ay = fabs(y), s;
    if (ax == 0.0 && ay == 0.0)
        return 0.0;
    if (ow_dexp(ax) == 0x7FF || ow_dexp(ay) == 0x7FF)
        return ax > ay ? ax : ay;
    s = ax > ay ? ax : ay;
    return s * sqrt((ax / s) * (ax / s) + (ay / s) * (ay / s));
}

/* CRT spelling (mingw math.h maps hypot -> _hypot, a plain extern). */
double _hypot(double x, double y)
{
    return hypot(x, y);
}

/* ---- float wrappers ------------------------------------------------ */
/* (implemented by promotion; x86-64 has no FP libcalls)                */

float  fabsf(float x)      { return (float)fabs((double)x); }
float  copysignf(float x, float y) { return (float)copysign((double)x, (double)y); }
float  ceilf(float x)      { return (float)ceil((double)x); }
float  floorf(float x)     { return (float)floor((double)x); }
float  truncf(float x)     { return (float)trunc((double)x); }
float  roundf(float x)     { return (float)round((double)x); }
float  rintf(float x)      { return (float)rint((double)x); }
float  nearbyintf(float x) { return (float)nearbyint((double)x); }
long   lrintf(float x)     { return lrint((double)x); }
long long llrintf(float x) { return llrint((double)x); }
float  fminf(float a, float b)   { return (float)fmin((double)a, (double)b); }
float  fmaxf(float a, float b)   { return (float)fmax((double)a, (double)b); }
float  fdimf(float a, float b)   { return (float)fdim((double)a, (double)b); }
float  fmodf(float x, float y)   { return (float)fmod((double)x, (double)y); }
float  remainderf(float x, float y) { return (float)remainder((double)x, (double)y); }
float  ldexpf(float x, int e)    { return (float)ldexp((double)x, e); }
float  scalbnf(float x, int e)   { return (float)scalbn((double)x, e); }
float  frexpf(float x, int *e)   { return (float)frexp((double)x, e); }
float  modff(float x, float *i)  { double di; float f = (float)modf((double)x, &di); if (i) *i = (float)di; return f; }
float  cbrtf(float x)      { return (float)cbrt((double)x); }
float  expf(float x)       { return (float)exp((double)x); }
float  exp2f(float x)      { return (float)exp2((double)x); }
float  logf(float x)       { return (float)log((double)x); }
float  log2f(float x)      { return (float)log2((double)x); }
float  log10f(float x)     { return (float)log10((double)x); }
float  log1pf(float x)     { return (float)log1p((double)x); }
float  powf(float x, float y)    { return (float)pow((double)x, (double)y); }
float  sinf(float x)       { return (float)sin((double)x); }
float  cosf(float x)       { return (float)cos((double)x); }
float  tanf(float x)       { return (float)tan((double)x); }
float  asinf(float x)      { return (float)asin((double)x); }
float  acosf(float x)      { return (float)acos((double)x); }
float  atanf(float x)      { return (float)atan((double)x); }
float  atan2f(float y, float x)  { return (float)atan2((double)y, (double)x); }
float  hypotf(float x, float y)  { return (float)hypot((double)x, (double)y); }