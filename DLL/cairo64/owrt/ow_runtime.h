/*
 * ow_runtime.h — OpenWindows freestanding C runtime interposition shim.
 *
 * The genuine cairo-1.18.2 and pixman-0.44.2 objects reference standard
 * libc/math symbols (malloc, memcpy, pow, snprintf, ...). This project is
 * freestanding C99 with -nostdlib, so we provide those symbols ourselves,
 * backed by the kernel64.owd allocator (k64_request_memory_block).
 *
 * Every symbol in this header is a definition we supply in DLL/cairo64/owrt.
 * All definitions are declared here FIRST so GCC's builtin prototypes never
 * conflict with our definitions (the runtime TUs are built with -fno-builtin).
 */

#ifndef OW_RUNTIME_H
#define OW_RUNTIME_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <stdarg.h>

#define OW_RT_VERSION 1u

/* ---- allocation ---------------------------------------------------- */
void  *malloc(size_t size);
void  *calloc(size_t nmemb, size_t size);
void  *realloc(void *ptr, size_t size);
void   free(void *ptr);

/* ---- memory -------------------------------------------------------- */
void  *memcpy(void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset(void *dst, int c, size_t n);
int    memcmp(const void *a, const void *b, size_t n);
void  *memchr(const void *s, int c, size_t n);

/* ---- strings ------------------------------------------------------- */
size_t strlen(const char *s);
size_t strnlen(const char *s, size_t maxlen);
int    strcmp(const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strchr(const char *s, int c);
char  *strrchr(const char *s, int c);
char  *strstr(const char *hay, const char *needle);
char  *strncpy(char *dst, const char *src, size_t n);
char  *strcpy(char *dst, const char *src);
char  *strcat(char *dst, const char *src);
char  *strncat(char *dst, const char *src, size_t n);
size_t strcspn(const char *s, const char *reject);
size_t strspn(const char *s, const char *accept);
char  *strpbrk(const char *s, const char *accept);

void   qsort(void *base, size_t nmemb, size_t size,
             int (*compar)(const void *, const void *));

/* ---- printf family -------------------------------------------------- */
int    snprintf(char *buf, size_t n, const char *fmt, ...);
int    vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
#if defined(__GNUC__)
    __attribute__((format(printf, 3, 0)))
#endif
    ;
int    sprintf(char *buf, const char *fmt, ...);

/* ---- stdio file ops (no files: ordered to fail safely ---------------- *
 * The `<stdio.h>` shim typedefs `FILE` as `void`, so these signatures use
 * `void *` stream handles; call-sites pass `FILE *` which is the same. */
void  *fopen(const char *path, const char *mode);
int    fclose(void *stream);
int    fflush(void *stream);
int    ferror(void *stream);
int    fputs(const char *s, void *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, void *stream);
void  *fdopen(int fd, const char *mode);

/* ---- numeric text parsing ------------------------------------------- */
double strtod(const char *nptr, char **endptr);
long   strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long   strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
int    atoi(const char *nptr);
double atof(const char *nptr);
int    abs(int x);
long   labs(long x);
long long llabs(long long x);

/* ---- interposition add-ons (ow_crt.c) -------------------------------- */
/* Externals the genuine Cairo/pixman objects reference beyond the core
 * libc subset above: ctype thunks, printf-family redirect names, strdup,
 * errno access, environment, and the "_hypot" CRT spelling of hypot. */
int    isdigit(int c);
int    isspace(int c);
char  *_strdup(const char *s);
char  *strdup(const char *s);
int    __mingw_printf(const char *fmt, ...);
int    __mingw_fprintf(void *stream, const char *fmt, ...);
int    __mingw_vprintf(const char *fmt, va_list ap);
int    __mingw_sprintf(char *buf, const char *fmt, ...);
int    printf(const char *fmt, ...);
int    fprintf(void *stream, const char *fmt, ...);
int    vfprintf(void *stream, const char *fmt, va_list ap);
void   abort(void);
void   exit(int status);
int    __mingw_sscanf(const char *s, const char *fmt, ...);
double __mingw_strtod(const char *nptr, char **endptr);
int    rand(void);
void   srand(unsigned int seed);
char  *getenv(const char *name);
int   *_errno(void);
double _hypot(double x, double y);

/* ---- libm: double and float ------------------------------------------ */
double fabs(double x);
double copysign(double x, double y);
double ceil(double x);
double floor(double x);
double trunc(double x);
double round(double x);
double rint(double x);
double nearbyint(double x);
long   lrint(double x);
long long llrint(double x);
double fmin(double a, double b);
double fmax(double a, double b);
double fdim(double a, double b);
double fmod(double x, double y);
double remainder(double x, double y);
double ldexp(double x, int e);
double scalbn(double x, int e);
double frexp(double x, int *e);
double modf(double x, double *i);
double sqrt(double x);
double cbrt(double x);
double exp(double x);
double exp2(double x);
double log(double x);
double log2(double x);
double log10(double x);
double log1p(double x);
double pow(double x, double y);
double sin(double x);
double cos(double x);
double tan(double x);
double asin(double x);
double acos(double x);
double atan(double x);
double atan2(double y, double x);
double hypot(double x, double y);

float  fabsf(float x);
float  copysignf(float x, float y);
float  ceilf(float x);
float  floorf(float x);
float  truncf(float x);
float  roundf(float x);
float  rintf(float x);
float  nearbyintf(float x);
long   lrintf(float x);
long long llrintf(float x);
float  fminf(float a, float b);
float  fmaxf(float a, float b);
float  fdimf(float a, float b);
float  fmodf(float x, float y);
float  remainderf(float x, float y);
float  ldexpf(float x, int e);
float  scalbnf(float x, int e);
float  frexpf(float x, int *e);
float  modff(float x, float *i);
float  sqrtf(float x);
float  cbrtf(float x);
float  expf(float x);
float  exp2f(float x);
float  logf(float x);
float  log2f(float x);
float  log10f(float x);
float  log1pf(float x);
float  powf(float x, float y);
float  sinf(float x);
float  cosf(float x);
float  tanf(float x);
float  asinf(float x);
float  acosf(float x);
float  atanf(float x);
float  atan2f(float y, float x);
float  hypotf(float x, float y);

#endif /* OW_RUNTIME_H */