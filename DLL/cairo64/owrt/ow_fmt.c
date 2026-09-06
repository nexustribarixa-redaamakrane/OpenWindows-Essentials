/*
 * ow_fmt.c — freestanding snprintf/vsnprintf + strtol/strtod family.
 * Compiled -nostdlib -fno-builtin. See ow_runtime.h.
 */
#include "ow_runtime.h"
#include <stdarg.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* output sink                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *buf;
    size_t size;
    size_t len;
} ow_sink_t;

static void ow_out(ow_sink_t *s, char c)
{
    if (s->len + 1 < s->size)
        s->buf[s->len] = c;
    s->len++;
}

static void ow_puts(ow_sink_t *s, const char *str, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i)
        ow_out(s, str[i]);
}

static void ow_term(ow_sink_t *s)
{
    if (s->size > 0)
        s->buf[s->len < s->size - 1 ? s->len : s->size - 1] = 0;
}

/* ------------------------------------------------------------------ */
/* double helpers                                                      */
/* ------------------------------------------------------------------ */

static int ow_signbit_d(double x)
{
    uint64_t u;
    memcpy(&u, &x, sizeof u);
    return (int)(u >> 63);
}

static int ow_isnan_d(double x)
{
    uint64_t u;
    memcpy(&u, &x, sizeof u);
    u &= UINT64_C(0x7FFFFFFFFFFFFFFF);
    return u > UINT64_C(0x7FF0000000000000);
}

static int ow_isinf_d(double x)
{
    uint64_t u;
    memcpy(&u, &x, sizeof u);
    u &= UINT64_C(0x7FFFFFFFFFFFFFFF);
    return u == UINT64_C(0x7FF0000000000000);
}

static double ow_pow10i(int e)
{
    double r = 1.0, b = 10.0;
    unsigned int n = (unsigned int)(e < 0 ? -e : e);
    while (n) {
        if (n & 1u)
            r *= b;
        b *= b;
        n >>= 1u;
    }
    return e < 0 ? 1.0 / r : r;
}

#define OW_FDIGCAP 17

/*
 * Produce `prec` significant digits of |x| in digs[] and the decimal
 * exponent in *expp such that |x| == 0.d0 d1 ... d(prec-1) x 10^exp.
 * Correctly rounds at the `prec`-th significant digit (half-up).
 */
static void ow_digits(double x, int prec, int *expp, char *digs)
{
    double v = x, scale, t;
    uint64_t i;
    int e = 0, k;

    if (!(v > 0.0)) {
        for (k = 0; k < prec; ++k)
            digs[k] = '0';
        *expp = 0;
        return;
    }
    if (prec > OW_FDIGCAP)
        prec = OW_FDIGCAP;
    if (prec < 1)
        prec = 1;

    if (v >= 1e18 || v <= 1e-15) {
        e = (int)floor(log10(v));
        if (e > 300)
            e = 300;
        if (e < -300)
            e = -300;
        v /= ow_pow10i(e);
    }
    while (v >= 10.0) { v /= 10.0; e++; }
    while (v < 1.0)   { v *= 10.0; e--; }

    scale = ow_pow10i(prec - 1);
    t = v * scale;
    i = (uint64_t)(t + 0.5);
    if (i >= (uint64_t)(scale * 10.0)) {
        i /= 10;
        e++;
    }
    for (k = prec - 1; k >= 0; --k) {
        digs[k] = (char)('0' + (int)(i % 10));
        i /= 10;
    }
    *expp = e;
}

/* ------------------------------------------------------------------ */
/* double formatting body                                               */
/* Writes digits (no sign, no padding) into body[]; sets *bneg.        */
/* ------------------------------------------------------------------ */

#define OW_BODYCAP 64

static void ow_body_double(double v, char mode, int prec, int flags,
                           char *body, size_t *blen, int *bneg)
{
    char digs[OW_FDIGCAP];
    int  e2, n, k;
    size_t len;

    *bneg = 0;
    if (ow_isnan_d(v)) {
        body[0] = 'n'; body[1] = 'a'; body[2] = 'n';
        *blen = 3;
        return;
    }
    if (ow_isinf_d(v)) {
        *bneg = ow_signbit_d(v);
        body[0] = 'i'; body[1] = 'n'; body[2] = 'f';
        *blen = 3;
        return;
    }
    if (ow_signbit_d(v) && v != 0.0)
        *bneg = 1;

    if (mode == 'e') {
        n = prec + 1;
        ow_digits(v, n, &e2, digs);
        for (len = 0; len < (size_t)OW_BODYCAP;) {
            body[len++] = digs[0];
            break;
        }
        if (prec > 0) {
            body[len++] = '.';
            for (k = 1; k <= prec && len < OW_BODYCAP; ++k)
                body[len++] = digs[k];
        }
        body[len++] = 'e';
        body[len++] = e2 < 0 ? '-' : '+';
        {
            int a = e2 < 0 ? -e2 : e2;
            if (a < 10) {
                body[len++] = '0';
                body[len++] = (char)('0' + a);
            } else if (a < 100) {
                body[len++] = (char)('0' + a / 10);
                body[len++] = (char)('0' + a % 10);
            } else {
                body[len++] = (char)('0' + (a / 100) % 10);
                body[len++] = (char)('0' + (a / 10) % 10);
                body[len++] = (char)('0' + a % 10);
            }
        }
        *blen = len;
        return;
    }

    if (mode == 'f') {
        int intdig;
        ow_digits(v, OW_FDIGCAP, &e2, digs);
        intdig = e2 + 1;
        if (intdig < 1)
            intdig = 1;
        n = intdig + prec;
        if (n > OW_FDIGCAP)
            n = OW_FDIGCAP;
        ow_digits(v, n, &e2, digs);

        len = 0;
        if (e2 >= 0) {
            for (k = 0; k <= e2 && k < n && len < OW_BODYCAP; ++k)
                body[len++] = digs[k];
            while ((int)len <= e2 && len < OW_BODYCAP)
                body[len++] = '0';
        } else {
            body[len++] = '0';
        }
        if (prec > 0 || (flags & 8)) {
            body[len++] = '.';
            if (e2 < 0) {
                int z = -e2 - 1;
                if (z > 40)
                    z = 40;
                while (z-- > 0 && len < OW_BODYCAP)
                    body[len++] = '0';
            }
            for (k = e2 >= 0 ? e2 + 1 : 0; k < n && len < OW_BODYCAP; ++k)
                body[len++] = digs[k];
            while ((int)len < e2 + 2 + prec && len < OW_BODYCAP)
                body[len++] = '0';
        }
        *blen = len;
        return;
    }

    /* mode 'g' */
    {
        int p = prec == 0 ? 1 : prec;
        int trav;
        ow_digits(v, OW_FDIGCAP, &e2, digs);
        trav = (e2 < -4 || e2 >= p);
        if (trav) {
            n = p;
            ow_digits(v, n, &e2, digs);
            len = 0;
            body[len++] = digs[0];
            if (n > 1 && len < OW_BODYCAP)
                body[len++] = '.';
            for (k = 1; k < n && len < OW_BODYCAP; ++k)
                body[len++] = digs[k];
            if (!(flags & 8)) {
                while (len > 0 && body[len - 1] == '0')
                    --len;
                if (len > 0 && body[len - 1] == '.')
                    --len;
            }
            body[len++] = 'e';
            body[len++] = e2 < 0 ? '-' : '+';
            {
                int a = e2 < 0 ? -e2 : e2;
                if (a < 10) {
                    body[len++] = '0';
                    body[len++] = (char)('0' + a);
                } else if (a < 100) {
                    body[len++] = (char)('0' + a / 10);
                    body[len++] = (char)('0' + a % 10);
                } else {
                    body[len++] = (char)('0' + (a / 100) % 10);
                    body[len++] = (char)('0' + (a / 10) % 10);
                    body[len++] = (char)('0' + a % 10);
                }
            }
            *blen = len;
            return;
        }
        {
            int dec = p - 1 - e2;
            if (dec < 0)
                dec = 0;
            n = e2 + 1 + dec;
            if (n < 1)
                n = 1;
            if (n > OW_FDIGCAP)
                n = OW_FDIGCAP;
            ow_digits(v, n, &e2, digs);
            len = 0;
            if (e2 >= 0) {
                for (k = 0; k <= e2 && k < n && len < OW_BODYCAP; ++k)
                    body[len++] = digs[k];
                while ((int)len <= e2 && len < OW_BODYCAP)
                    body[len++] = '0';
            } else {
                body[len++] = '0';
            }
            if (dec > 0) {
                body[len++] = '.';
                if (e2 < 0) {
                    int z = -e2 - 1;
                    if (z > 40)
                        z = 40;
                    while (z-- > 0 && len < OW_BODYCAP)
                        body[len++] = '0';
                }
                for (k = e2 >= 0 ? e2 + 1 : 0; k < n && len < OW_BODYCAP; ++k)
                    body[len++] = digs[k];
                if (!(flags & 8)) {
                    while (len > 0 && body[len - 1] == '0')
                        --len;
                    if (len > 0 && body[len - 1] == '.')
                        --len;
                }
            } else if (!(flags & 8)) {
                while (len > 0 && body[len - 1] == '0')
                    --len;
            }
            *blen = len;
            return;
        }
    }
}

#define OW_MINUS 1
#define OW_PLUS  2
#define OW_SPACE 4
#define OW_HASH  8
#define OW_ZERO  16

/* ------------------------------------------------------------------ */
/* vsnprintf                                                           */
/* ------------------------------------------------------------------ */

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
    ow_sink_t s;
    const char *p = fmt;
    s.buf = buf;
    s.size = n;
    s.len = 0;

    while (*p) {
        char c = *p++;
        int flags = 0, width = 0, prec = -1;
        char lenmod = 0;

        if (c != '%') {
            ow_out(&s, c);
            continue;
        }

        for (;;) {
            c = *p++;
            if (c == '-') flags |= OW_MINUS;
            else if (c == '+') flags |= OW_PLUS;
            else if (c == ' ') flags |= OW_SPACE;
            else if (c == '#') flags |= OW_HASH;
            else if (c == '0') flags |= OW_ZERO;
            else break;
        }
        if (c == '*') {
            width = va_arg(ap, int);
            if (width < 0) { flags |= OW_MINUS; width = -width; }
            c = *p++;
        } else {
            while (c >= '0' && c <= '9') {
                width = width * 10 + (c - '0');
                c = *p++;
            }
        }
        if (c == '.') {
            c = *p++;
            if (c == '*') {
                prec = va_arg(ap, int);
                c = *p++;
            } else {
                prec = 0;
                while (c >= '0' && c <= '9') {
                    prec = prec * 10 + (c - '0');
                    c = *p++;
                }
            }
        }
        while (c == 'h' || c == 'l' || c == 'L' || c == 'j' ||
               c == 'z' || c == 't') {
            if (c == 'l') {
                if (lenmod == 0)
                    lenmod = 'l';
                else if (lenmod == 'l')
                    lenmod = 'L';
            } else if (c == 'h') {
                lenmod = 'h';
            } else {
                lenmod = c;
            }
            c = *p++;
        }
        if (c == 0)
            break;

        switch (c) {
        case 'd':
        case 'i': {
            intmax_t v;
            int neg = 0;
            uintmax_t u;
            char tmp[21];
            int nd = 0, zp = 0, total, pad, signlen;
            if (lenmod == 'L') v = va_arg(ap, long long);
            else if (lenmod == 'l') v = va_arg(ap, long);
            else if (lenmod == 'h') v = (short)va_arg(ap, int);
            else if (lenmod == 'j') v = va_arg(ap, intmax_t);
            else if (lenmod == 'z' || lenmod == 't') v = (intmax_t)va_arg(ap, ptrdiff_t);
            else v = va_arg(ap, int);
            if (v < 0) { neg = 1; u = (uintmax_t)(-(v + 1)) + 1u; }
            else u = (uintmax_t)v;
            do { tmp[nd++] = (char)('0' + (int)(u % 10)); u /= 10; } while (u);
            if (prec == 0 && v == 0)
                nd = 0;
            while (prec > nd)
                ++zp, --prec;
            signlen = neg ? 1 : (flags & (OW_PLUS | OW_SPACE) ? 1 : 0);
            total = nd + zp + signlen;
            pad = width > total ? width - total : 0;
            if (!(flags & OW_MINUS) && ((flags & OW_ZERO) == 0 || prec >= 0))
                while (pad-- > 0) ow_out(&s, ' ');
            if (neg) ow_out(&s, '-');
            else if (flags & OW_PLUS) ow_out(&s, '+');
            else if (flags & OW_SPACE) ow_out(&s, ' ');
            if (!(flags & OW_MINUS) && (flags & OW_ZERO) && prec < 0)
                while (pad-- > 0) ow_out(&s, '0');
            while (zp-- > 0) ow_out(&s, '0');
            while (nd-- > 0) ow_out(&s, tmp[nd]);
            if (flags & OW_MINUS)
                while (pad-- > 0) ow_out(&s, ' ');
            break;
        }
        case 'u':
        case 'o':
        case 'x':
        case 'X': {
            uintmax_t u;
            unsigned base = (c == 'u') ? 10 : (c == 'o') ? 8 : 16;
            int up = (c == 'X');
            char tmp[23];
            int nd = 0, zp = 0, total, pad, pref = 0;
            if (lenmod == 'L') u = va_arg(ap, unsigned long long);
            else if (lenmod == 'l') u = va_arg(ap, unsigned long);
            else if (lenmod == 'h') u = (unsigned short)va_arg(ap, unsigned);
            else if (lenmod == 'j') u = va_arg(ap, uintmax_t);
            else if (lenmod == 'z' || lenmod == 't') u = (uintmax_t)va_arg(ap, size_t);
            else u = va_arg(ap, unsigned);
            do {
                unsigned d = (unsigned)(u % base);
                tmp[nd++] = (char)(d < 10 ? '0' + d : (up ? 'A' : 'a') + (d - 10));
                u /= base;
            } while (u);
            if (prec == 0 && tmp[0] == '0' && nd == 1)
                nd = 0;
            while (prec > nd)
                ++zp, --prec;
            if (c == 'o' && (flags & OW_HASH) && zp > 0)
                --zp;
            if (flags & OW_HASH) {
                if (c == 'x' || c == 'X')
                    pref = (nd > 0 && tmp[nd - 1] != '0') ? 2 : 0;
                else if (c == 'o' && nd > 0 && tmp[nd - 1] != '0')
                    pref = 1;
            }
            total = nd + zp + pref;
            pad = width > total ? width - total : 0;
            if (!(flags & OW_MINUS) && ((flags & OW_ZERO) == 0 || prec >= 0))
                while (pad-- > 0) ow_out(&s, ' ');
            if (pref == 2) ow_puts(&s, up ? "0X" : "0x", 2);
            if (pref == 1) ow_out(&s, '0');
            if (!(flags & OW_MINUS) && (flags & OW_ZERO) && prec < 0)
                while (pad-- > 0) ow_out(&s, '0');
            while (zp-- > 0) ow_out(&s, '0');
            while (nd-- > 0) ow_out(&s, tmp[nd]);
            if (flags & OW_MINUS)
                while (pad-- > 0) ow_out(&s, ' ');
            break;
        }
        case 'c': {
            int ch = va_arg(ap, int);
            int pad = width > 1 ? width - 1 : 0;
            if (!(flags & OW_MINUS))
                while (pad-- > 0) ow_out(&s, ' ');
            ow_out(&s, (char)ch);
            if (flags & OW_MINUS)
                while (pad-- > 0) ow_out(&s, ' ');
            break;
        }
        case 's': {
            const char *ss = va_arg(ap, const char *);
            size_t ln;
            int pad;
            if (!ss)
                ss = "(null)";
            ln = strlen(ss);
            if (prec >= 0 && (size_t)prec < ln)
                ln = (size_t)prec;
            pad = width > (int)ln ? width - (int)ln : 0;
            if (!(flags & OW_MINUS))
                while (pad-- > 0) ow_out(&s, ' ');
            ow_puts(&s, ss, ln);
            if (flags & OW_MINUS)
                while (pad-- > 0) ow_out(&s, ' ');
            break;
        }
        case 'p': {
            uintptr_t u = (uintptr_t)va_arg(ap, void *);
            char tmp[18];
            int nd = 0;
            if (u == 0) { ow_puts(&s, "(nil)", 5); break; }
            do {
                unsigned d = (unsigned)(u & 0xFu);
                tmp[nd++] = (char)(d < 10 ? '0' + d : 'a' + (d - 10));
                u >>= 4;
            } while (u);
            ow_puts(&s, "0x", 2);
            while (nd > 0)
                ow_out(&s, tmp[--nd]);
            break;
        }
        case '%':
            ow_out(&s, '%');
            break;
        case 'f':
        case 'F':
        case 'e':
        case 'E':
        case 'g':
        case 'G': {
            double v = va_arg(ap, double);
            char body[OW_BODYCAP];
            size_t blen;
            int bneg, total, pad, signlen, i;
            char mode = (char)((c == 'F') ? 'f' : (c == 'E') ? 'e' : (c == 'G') ? 'g' : c);
            if (prec < 0)
                prec = 6;
            if (prec > OW_FDIGCAP)
                prec = OW_FDIGCAP;
            ow_body_double(v, mode, prec, flags, body, &blen, &bneg);
            signlen = bneg ? 1 : (flags & (OW_PLUS | OW_SPACE) ? 1 : 0);
            total = (int)blen + signlen;
            pad = width > total ? width - total : 0;
            if (!(flags & OW_MINUS)) {
                if (flags & OW_ZERO) {
                    if (bneg) ow_out(&s, '-');
                    else if (flags & OW_PLUS) ow_out(&s, '+');
                    else if (flags & OW_SPACE) ow_out(&s, ' ');
                    while (pad-- > 0) ow_out(&s, '0');
                } else {
                    while (pad-- > 0) ow_out(&s, ' ');
                    if (bneg) ow_out(&s, '-');
                    else if (flags & OW_PLUS) ow_out(&s, '+');
                    else if (flags & OW_SPACE) ow_out(&s, ' ');
                }
            } else {
                if (bneg) ow_out(&s, '-');
                else if (flags & OW_PLUS) ow_out(&s, '+');
                else if (flags & OW_SPACE) ow_out(&s, ' ');
            }
            for (i = 0; i < (int)blen; ++i)
                ow_out(&s, body[i]);
            if (flags & OW_MINUS)
                while (pad-- > 0) ow_out(&s, ' ');
            break;
        }
        default:
            ow_out(&s, '%');
            ow_out(&s, c);
            break;
        }
    }
    ow_term(&s);
    return s.len > 0x7FFFFFFFu ? (int)0x7FFFFFFF : (int)s.len;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, 0x7FFFFFFFu, fmt, ap);
    va_end(ap);
    return r;
}

/* ------------------------------------------------------------------ */
/* strtol family                                                       */
/* ------------------------------------------------------------------ */

static int ow_digitval(int c, int base)
{
    int v;
    if (c >= '0' && c <= '9') v = c - '0';
    else if (c >= 'a' && c <= 'z') v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z') v = c - 'A' + 10;
    else return -1;
    return v < base ? v : -1;
}

static unsigned long long ow_strtoull(const char *nptr, char **endptr,
                                      int base, unsigned long long hmax)
{
    const char *p = nptr;
    int neg = 0, ovf = 0;
    unsigned long long acc = 0;
    int d;

    while (isspace((unsigned char)*p)) ++p;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); ++p; }
    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16; p += 2;
    } else if (base == 0) {
        base = (p[0] == '0' && (p[1] >= '0' && p[1] <= '7')) ? 8 : 10;
    }
    for (;;) {
        d = ow_digitval((unsigned char)*p, base);
        if (d < 0)
            break;
        if (acc > (hmax - (unsigned long long)d) / (unsigned long long)base)
            ovf = 1;
        else
            acc = acc * (unsigned long long)base + (unsigned long long)d;
        ++p;
    }
    if (endptr) *endptr = (char *)p;
    if (ovf)
        return hmax;
    return neg ? (unsigned long long)0 - acc : acc;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base)
{
    return ow_strtoull(nptr, endptr, base, (unsigned long long)-1);
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    return (unsigned long)ow_strtoull(nptr, endptr, base, 0xFFFFFFFFull);
}

long long strtoll(const char *nptr, char **endptr, int base)
{
    return (long long)ow_strtoull(nptr, endptr, base, 0x7FFFFFFFFFFFFFFFull);
}

long strtol(const char *nptr, char **endptr, int base)
{
    return (long)ow_strtoull(nptr, endptr, base, 0x7FFFFFFFull);
}

int atoi(const char *nptr)
{
    return (int)strtol(nptr, 0, 10);
}

double atof(const char *nptr)
{
    return strtod(nptr, 0);
}

/* ------------------------------------------------------------------ */
/* strtod                                                              */
/* ------------------------------------------------------------------ */

double strtod(const char *nptr, char **endptr)
{
    const char *p = nptr;
    int neg = 0, seen = 0, frac = 0, e10 = 0, esign = 1, eok = 0;
    double m = 0.0;

    while (isspace((unsigned char)*p)) ++p;
    if (*p == '+' || *p == '-') { neg = (*p == '-'); ++p; }

    if (p[0] == 'i' || p[0] == 'I') {
        if ((p[1] == 'n' || p[1] == 'N') && (p[2] == 'f' || p[2] == 'F')) {
            const char *q = p + 3;
            if ((q[0] == 'i' || q[0] == 'I') && (q[1] == 'n' || q[1] == 'N') &&
                (q[2] == 'i' || q[2] == 'I') && (q[3] == 't' || q[3] == 'T') &&
                (q[4] == 'y' || q[4] == 'Y'))
                q += 5;
            if (endptr) *endptr = (char *)q;
            return neg ? -(1.0 / 0.0) : (1.0 / 0.0);
        }
    }
    if (p[0] == 'n' || p[0] == 'N') {
        if ((p[1] == 'a' || p[1] == 'A') && (p[2] == 'n' || p[2] == 'N')) {
            if (endptr) *endptr = (char *)(p + 3);
            return neg ? -0.0 / 0.0 : 0.0 / 0.0;
        }
    }

    while (isdigit((unsigned char)*p)) {
        m = m * 10.0 + (double)(*p - '0');
        seen = 1;
        ++p;
    }
    if (*p == '.') {
        ++p;
        while (isdigit((unsigned char)*p)) {
            if (seen || m != 0.0) {
                m = m * 10.0 + (double)(*p - '0');
                frac++;
                seen = 1;
            } else {
                frac++;   /* leading zeros after the point */
            }
            ++p;
        }
    }
    if (!seen) {
        if (endptr) *endptr = (char *)nptr;
        return 0.0;
    }
    if (frac > 320)
        frac = 320;

    if (*p == 'e' || *p == 'E') {
        const char *q = p + 1;
        int sn = 1, di = 0;
        if (*q == '+' || *q == '-') { sn = (*q == '-') ? -1 : 1; ++q; }
        while (isdigit((unsigned char)*q)) {
            if (di < 4)
                e10 = e10 * 10 + (*q - '0');
            ++q; ++di;
        }
        if (di > 0) {
            p = q;
            esign = sn;
            eok = 1;
        }
    }
    if (endptr) *endptr = (char *)p;

    {
        int E = (eok ? esign * e10 : 0) - frac;
        double m10 = (m == 0.0) ? 0.0 : m;
        if (E > 308)
            E = 308;
        if (E < -308)
            E = -308;
        if (E != 0)
            m10 *= ow_pow10i(E);
        return neg ? -m10 : m10;
    }
}