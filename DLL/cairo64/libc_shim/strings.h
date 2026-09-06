#ifndef _SHIM_STRINGS_H_
#define _SHIM_STRINGS_H_
#include "string.h"
#include "ctype.h"
static inline int strcasecmp(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower(*(const unsigned char *)s1);
        int c2 = tolower(*(const unsigned char *)s2);
        if (c1 != c2) return c1 - c2;
        s1++; s2++;
    }
    return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}
static inline int strncasecmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int c1 = tolower(*(const unsigned char *)(s1 + i));
        int c2 = tolower(*(const unsigned char *)(s2 + i));
        if (c1 != c2 || s1[i] == '\0') return c1 - c2;
    }
    return 0;
}
#endif
