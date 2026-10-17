/*
 * owstring64.c - OpenWindows String Operations Library (.owd)
 *
 * Bounded string primitives and a compact formatter that writes into a
 * caller-provided buffer. Nothing here allocates memory.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owstring64.h"

/* ── Formatter output state ───────────────────────────────────── */

typedef struct {
    char   *dst;
    size_t  cap;   /* total capacity including the terminator */
    size_t  len;   /* characters actually stored so far        */
} owstr_out_t;

/* Append one character, reserving one byte for the terminator. */
static void out_char(owstr_out_t *o, char c)
{
    if (o->cap == 0u) return;
    if (o->len + 1u < o->cap) {
        o->dst[o->len] = c;
        o->len++;
    }
}

static void out_str(owstr_out_t *o, const char *s)
{
    if (s == NULL) s = "(null)";
    while (*s != '\0') {
        out_char(o, *s);
        s++;
    }
}

static void out_uint(owstr_out_t *o, uint64_t v, uint32_t base, bool upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[64];
    uint32_t n = 0u;

    if (v == 0u) {
        out_char(o, '0');
        return;
    }
    while (v != 0u && n < (uint32_t)sizeof(tmp)) {
        tmp[n] = digits[(uint32_t)(v % (uint64_t)base)];
        n++;
        v /= (uint64_t)base;
    }
    while (n > 0u) {
        n--;
        out_char(o, tmp[n]);
    }
}

static void out_int(owstr_out_t *o, int64_t v)
{
    uint64_t magnitude;
    if (v < 0) {
        out_char(o, '-');
        /* INT64_MIN safe magnitude computation. */
        magnitude = (uint64_t)(-(v + 1)) + 1u;
    } else {
        magnitude = (uint64_t)v;
    }
    out_uint(o, magnitude, 10u, false);
}

static void out_ptr(owstr_out_t *o, const void *p)
{
    out_char(o, '0');
    out_char(o, 'x');
    out_uint(o, (uint64_t)(uintptr_t)p, 16u, false);
}

/* ── String primitives ────────────────────────────────────────── */

size_t owstr_len(const char *s)
{
    if (s == NULL) return 0u;
    size_t n = 0u;
    while (s[n] != '\0') n++;
    return n;
}

size_t owstr_copy(char *dst, size_t dst_cap, const char *src)
{
    if (dst == NULL || dst_cap == 0u) return 0u;

    size_t i = 0u;
    if (src != NULL) {
        while (src[i] != '\0' && i + 1u < dst_cap) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
    return i;
}

int32_t owstr_compare(const char *a, const char *b)
{
    if (a == NULL) a = "";
    if (b == NULL) b = "";

    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return (*a < *b) ? -1 : 1;
        a++;
        b++;
    }
    if (*a == *b) return 0;
    return (*a == '\0') ? -1 : 1;
}

size_t owstr_concat(char *dst, size_t dst_cap, const char *src)
{
    if (dst == NULL || dst_cap == 0u) return 0u;

    size_t len = owstr_len(dst);
    if (len >= dst_cap) {
        dst[dst_cap - 1u] = '\0';
        len = dst_cap - 1u;
    }
    if (src == NULL) return len;

    size_t i = 0u;
    while (src[i] != '\0' && len + 1u < dst_cap) {
        dst[len] = src[i];
        len++;
        i++;
    }
    dst[len] = '\0';
    return len;
}

const char *owstr_find(const char *haystack, const char *needle)
{
    if (haystack == NULL || needle == NULL) return NULL;
    if (*needle == '\0') return haystack;

    for (size_t i = 0u; haystack[i] != '\0'; ++i) {
        size_t j = 0u;
        while (needle[j] != '\0' && haystack[i + j] == needle[j]) j++;
        if (needle[j] == '\0') return &haystack[i];
    }
    return NULL;
}

char *owstr_to_upper(char *s)
{
    if (s == NULL) return NULL;
    for (size_t i = 0u; s[i] != '\0'; ++i) {
        if (s[i] >= 'a' && s[i] <= 'z') {
            s[i] = (char)(s[i] - ('a' - 'A'));
        }
    }
    return s;
}

char *owstr_to_lower(char *s)
{
    if (s == NULL) return NULL;
    for (size_t i = 0u; s[i] != '\0'; ++i) {
        if (s[i] >= 'A' && s[i] <= 'Z') {
            s[i] = (char)(s[i] + ('a' - 'A'));
        }
    }
    return s;
}

/* ── Formatter ────────────────────────────────────────────────── */

size_t owstr_format(char *dst, size_t dst_cap, const char *fmt,
                    const owstr_arg_t *args, size_t arg_count)
{
    owstr_out_t o;
    o.dst = dst;
    o.cap = dst_cap;
    o.len = 0u;

    size_t arg_index = 0u;

    if (fmt != NULL) {
        size_t i = 0u;
        while (fmt[i] != '\0') {
            char c = fmt[i];
            i++;

            if (c != '%') {
                out_char(&o, c);
                continue;
            }

            char spec = fmt[i];
            if (spec == '\0') {
                out_char(&o, '%');
                break;
            }
            i++;

            if (spec == '%') {
                out_char(&o, '%');
                continue;
            }

            const owstr_arg_t *arg = NULL;
            if (arg_index < arg_count) arg = &args[arg_index];
            arg_index++;

            switch (spec) {
                case 'd':
                case 'i':
                    out_int(&o, (arg != NULL) ? arg->value.i : 0);
                    break;
                case 'u':
                    out_uint(&o, (arg != NULL) ? arg->value.u : 0u, 10u, false);
                    break;
                case 'x':
                    out_uint(&o, (arg != NULL) ? arg->value.u : 0u, 16u, false);
                    break;
                case 'X':
                    out_uint(&o, (arg != NULL) ? arg->value.u : 0u, 16u, true);
                    break;
                case 'c':
                    out_char(&o, (arg != NULL) ? arg->value.c : '?');
                    break;
                case 's':
                    out_str(&o, (arg != NULL) ? arg->value.s : "");
                    break;
                case 'p':
                    out_ptr(&o, (arg != NULL) ? arg->value.p : NULL);
                    break;
                default:
                    out_char(&o, '%');
                    out_char(&o, spec);
                    break;
            }
        }
    }

    if (dst != NULL && dst_cap != 0u) dst[o.len] = '\0';
    return o.len;
}
