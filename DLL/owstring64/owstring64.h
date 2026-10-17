/*
 * owstring64.h - OpenWindows String Operations Library (.owd)
 *
 * Bounded, allocation-free C string helpers plus a small printf-style
 * formatter. The formatter takes a caller-owned array of tagged
 * owstr_arg_t values instead of a variadic argument list so that it
 * stays within the freestanding include set.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSTRING64_H
#define OWSTRING64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Tag describing which union member an owstr_arg_t carries. */
typedef enum {
    OWSTR_ARG_INT = 0,   /* value.i : signed decimal        */
    OWSTR_ARG_UINT,      /* value.u : unsigned decimal      */
    OWSTR_ARG_HEX,       /* value.u : hexadecimal           */
    OWSTR_ARG_CHAR,      /* value.c : single character      */
    OWSTR_ARG_STR,       /* value.s : NUL-terminated string */
    OWSTR_ARG_PTR        /* value.p : pointer value         */
} owstr_arg_kind_t;

/* One formatter argument. */
typedef struct {
    owstr_arg_kind_t kind;
    union {
        int64_t     i;
        uint64_t    u;
        char        c;
        const char *s;
        const void *p;
    } value;
} owstr_arg_t;

/* Length of s in bytes, excluding the terminator. NULL yields 0. */
size_t owstr_len(const char *s);

/* Copy src into dst (always NUL-terminated when dst_cap > 0). Returns
 * the number of characters stored, excluding the terminator. */
size_t owstr_copy(char *dst, size_t dst_cap, const char *src);

/* Lexicographic comparison. Returns < 0, 0 or > 0. NULL is treated as
 * the empty string. */
int32_t owstr_compare(const char *a, const char *b);

/* Append src to dst (bounded by dst_cap). Returns the resulting
 * length, excluding the terminator. */
size_t owstr_concat(char *dst, size_t dst_cap, const char *src);

/* First occurrence of needle inside haystack, or NULL. An empty needle
 * matches at the start of haystack. */
const char *owstr_find(const char *haystack, const char *needle);

/* ASCII upper/lower case conversion in place; returns s. */
char *owstr_to_upper(char *s);
char *owstr_to_lower(char *s);

/* Minimal printf-style formatter. Supports %d %i %u %x %X %c %s %p and
 * %%. Arguments are consumed in order from args[0..arg_count). Returns
 * the number of characters written, excluding the terminator. */
size_t owstr_format(char *dst, size_t dst_cap, const char *fmt,
                    const owstr_arg_t *args, size_t arg_count);

#endif /* OWSTRING64_H */
