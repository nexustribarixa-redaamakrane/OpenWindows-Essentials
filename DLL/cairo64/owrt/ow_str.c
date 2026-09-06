/*
 * ow_str.c — freestanding string helpers and qsort.
 * Compiled -nostdlib -fno-builtin. See ow_runtime.h.
 */
#include "ow_runtime.h"

size_t strlen(const char *s)
{
    const char *p = s;
    while (*p)
        ++p;
    return (size_t)(p - s);
}

size_t strnlen(const char *s, size_t maxlen)
{
    size_t i = 0;
    while (i < maxlen && s[i])
        ++i;
    return i;
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b)
        ++a, ++b;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n)
{
    size_t i = 0;
    for (; i < n; ++i) {
        if (a[i] != b[i])
            return (int)(unsigned char)a[i] - (int)(unsigned char)b[i];
        if (a[i] == 0)
            break;
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    char ch = (char)c;
    for (;;) {
        if (*s == ch)
            return (char *)s;
        if (*s == 0)
            return 0;
        ++s;
    }
}

char *strrchr(const char *s, int c)
{
    char ch = (char)c;
    const char *last = 0;
    for (;;) {
        if (*s == ch)
            last = s;
        if (*s == 0)
            return (char *)last;
        ++s;
    }
}

char *strstr(const char *hay, const char *needle)
{
    const char *n;
    if (!*needle)
        return (char *)hay;
    for (; *hay; ++hay) {
        n = needle;
        if (*hay != *n)
            continue;
        while (*n && hay[n - needle] == *n)
            ++n;
        if (*n == 0)
            return (char *)hay;
    }
    return 0;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    while (i < n && src[i]) {
        dst[i] = src[i];
        ++i;
    }
    while (i < n)
        dst[i++] = 0;
    return dst;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;
    while ((*d++ = *src++) != 0)
        ;
    return dst;
}

char *strcat(char *dst, const char *src)
{
    char *d = dst;
    while (*d)
        ++d;
    while ((*d++ = *src++) != 0)
        ;
    return dst;
}

char *strncat(char *dst, const char *src, size_t n)
{
    char *d = dst;
    size_t i = 0;
    while (*d)
        ++d;
    while (i < n && src[i]) {
        d[i] = src[i];
        ++i;
    }
    d[i] = 0;
    return dst;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t i = 0;
    while (s[i]) {
        size_t j = 0;
        while (reject[j]) {
            if (s[i] == reject[j])
                return i;
            ++j;
        }
        ++i;
    }
    return i;
}

size_t strspn(const char *s, const char *accept)
{
    size_t i = 0;
    while (s[i]) {
        size_t j = 0;
        while (accept[j]) {
            if (s[i] == accept[j])
                break;
            ++j;
        }
        if (accept[j] == 0)
            break;
        ++i;
    }
    return i;
}

char *strpbrk(const char *s, const char *accept)
{
    const char *a;
    for (; *s; ++s) {
        for (a = accept; *a; ++a)
            if (*s == *a)
                return (char *)s;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* qsort — median-of-three quicksort with insertion-sort tail.         */
/* ------------------------------------------------------------------ */

typedef int (*ow_cmp_t)(const void *, const void *);

static void ow_swap(void *a, void *b, size_t size)
{
    unsigned char tmp[32];
    unsigned char *x = a, *y = b;
    while (size >= sizeof(tmp)) {
        memcpy(tmp, x, sizeof(tmp));
        memcpy(x, y, sizeof(tmp));
        memcpy(y, tmp, sizeof(tmp));
        x += sizeof(tmp);
        y += sizeof(tmp);
        size -= sizeof(tmp);
    }
    if (size) {
        memcpy(tmp, x, size);
        memcpy(x, y, size);
        memcpy(y, tmp, size);
    }
}

/* selection sort for the small-range base case (swap-only, no buffers) */
static void ow_selsort(unsigned char *b, size_t lo, size_t hi,
                       size_t size, ow_cmp_t cmp)
{
    size_t i, j, m;
    for (i = lo; i < hi; ++i) {
        m = i;
        for (j = i + 1; j < hi; ++j)
            if (cmp(b + j * size, b + m * size) < 0)
                m = j;
        if (m != i)
            ow_swap(b + i * size, b + m * size, size);
    }
}

static void ow_quicksort(void *base, size_t nmemb, size_t size, ow_cmp_t cmp)
{
    unsigned char *b = base;
    size_t stack[64];
    int sp = 0;
    size_t start, end;

    stack[sp++] = 0;
    stack[sp++] = nmemb;

    while (sp >= 2) {
        end = stack[--sp];
        start = stack[--sp];

        if (end - start <= 16u) {
            ow_selsort(b, start, end, size, cmp);
            continue;
        }

        {
            size_t mid = start + (end - start) / 2;
            void *pmid = b + mid * size;
            void *pstart = b + start * size;
            void *pend = b + (end - 1) * size;
            if (cmp(pmid, pstart) < 0) ow_swap(pmid, pstart, size);
            if (cmp(pend, pstart) < 0) ow_swap(pend, pstart, size);
            if (cmp(pend, pmid) < 0) ow_swap(pend, pmid, size);
            ow_swap(pmid, pstart, size);   /* median as pivot at front */
        }

        {
            size_t i = start + 1, j = end - 1;
            for (;;) {
                while (i < end && cmp(b + i * size, b + start * size) <= 0)
                    ++i;
                while (j > start && cmp(b + j * size, b + start * size) > 0)
                    --j;
                if (i >= j)
                    break;
                ow_swap(b + i * size, b + j * size, size);
                ++i;
                --j;
            }
            ow_swap(b + start * size, b + j * size, size);

            if (sp + 4 > (int)(sizeof(stack) / sizeof(stack[0]))) {
                /* stack bound never hit for sane partitions; guard anyway */
                ow_selsort(b, 0, nmemb, size, cmp);
                return;
            }
            stack[sp++] = start;
            stack[sp++] = (j > start) ? j : start + 1;
            stack[sp++] = i;
            stack[sp++] = end;
        }
    }
}

void qsort(void *base, size_t nmemb, size_t size,
           int (*compar)(const void *, const void *))
{
    if (nmemb == 0 || size == 0)
        return;
    ow_quicksort(base, nmemb, size, compar);
}

/* ------------------------------------------------------------------ */
/* abs family                                                           */
/* ------------------------------------------------------------------ */

int abs(int x)   { return x < 0 ? -x : x; }
long labs(long x) { return x < 0 ? -x : x; }
long long llabs(long long x) { return x < 0 ? -x : x; }