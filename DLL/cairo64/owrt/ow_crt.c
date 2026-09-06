/*
 * ow_crt.c — CRT/OS interposition surface for the vendored Cairo/pixman.
 * Compiled -nostdlib -fno-builtin. See ow_runtime.h.
 *
 * The genuine objects reference symbols that a hosted CRT supplies. This
 * module is freestanding with no console, no filesystem and no Win32, so:
 *   - pure-algorithm services (ctype, strdup, rand, snprintf, strtod,
 *     isdigit/isspace, errno) are implemented for real;
 *   - I/O services (printf to console, file create/temp, Win32 handles)
 *     have no backing and are ordered to fail safely.
 * The "<prefix>__imp_<name>" symbols are ms-ABI import thunks: the genuine
 * objects were compiled against mingw dllimport declarations and reach the
 * functions through a &__imp_x pointer cell. We supply both the function
 * and the cell so the PE link resolves without the CRT import library.
 */

#include "ow_runtime.h"
#include <stdarg.h>

/* Opaque stand-in for the CRT FILE type: the genuine objects never pass a
 * real descriptor body across our boundary (all file ops fail by design). */
typedef struct ow_file { unsigned char _c; } ow_file;
typedef ow_file FILE;

/* ---- errno ---------------------------------------------------------- */
int  ow_errno_val;
int *__imp__errno = &ow_errno_val;
int *_errno(void) { return &ow_errno_val; }

/* ---- ctype (dllimport thunks) --------------------------------------- */
int isdigit(int c)
{
    return c >= '0' && c <= '9';
}
int isspace(int c)
{
    return c == ' ' || (c >= '\t' && c <= '\r');
}
int (*__imp_isdigit)(int) = isdigit;
int (*__imp_isspace)(int) = isspace;

/* ---- string extras --------------------------------------------------- */
char *_strdup(const char *s)
{
    size_t n = strlen(s) + 1u;
    char *d = (char *)malloc(n);
    if (d)
        memcpy(d, s, n);
    return d;
}
char *(*__imp__strdup)(const char *) = _strdup;

char *strdup(const char *s)
{
    return _strdup(s);
}
char *(*__imp_strdup)(const char *) = strdup;

/* ---- printf family (mingw redirect names) ---------------------------- */
int __mingw_printf(const char *fmt, ...)
{
    (void)fmt;
    return -1;
}

int __mingw_fprintf(void *stream, const char *fmt, ...)
{
    (void)stream;
    (void)fmt;
    return -1;
}

int __mingw_vprintf(const char *fmt, va_list ap)
{
    (void)fmt;
    (void)ap;
    return -1;
}

int printf(const char *fmt, ...)
{
    (void)fmt;
    return -1;
}

int fprintf(void *stream, const char *fmt, ...)
{
    (void)stream;
    (void)fmt;
    return -1;
}

int vfprintf(void *stream, const char *fmt, va_list ap)
{
    (void)stream;
    (void)fmt;
    (void)ap;
    return -1;
}

void abort(void)
{
    while (1) { }
}

void exit(int status)
{
    (void)status;
    while (1) { }
}

int __mingw_sprintf(char *buf, const char *fmt, ...)
{
    va_list ap;
    int r;
    va_start(ap, fmt);
    r = vsnprintf(buf, 0x7FFFFFFFu, fmt, ap);
    va_end(ap);
    return r;
}

int (*__imp__snprintf)(char *, size_t, const char *, ...) = snprintf;

/*
 * Minimal scanner for the "sscanf" shape the vendored code uses
 * (%s/%d/%u/%x/%i/%f/%c/%n). Returns the number of successful conversions.
 */
int __mingw_sscanf(const char *s, const char *fmt, ...)
{
    const char *s0 = s;
    va_list ap;
    int matched = 0;
    va_start(ap, fmt);

    for (;;) {
        if (*fmt == ' ' || *fmt == '\t' || *fmt == '\n') {
            fmt++;
            continue;
        }
        if (*fmt == 0)
            break;
        if (*fmt != '%') {
            if (*s == *fmt) {
                s++;
                fmt++;
                continue;
            }
            break;
        }
        fmt++;
        while (*fmt == 'l' || *fmt == 'h' || *fmt == 'L')
            fmt++;

        if (*fmt == 'n') {
            int *np = va_arg(ap, int *);
            *np = (int)(s - s0);
            fmt++;
            continue;
        }
        if (*fmt == 'c') {
            char *cp = va_arg(ap, char *);
            if (*s == 0)
                break;
            *cp = *s++;
            matched++;
            fmt++;
            continue;
        }
        if (*fmt == 's') {
            char *op = va_arg(ap, char *);
            while (*s == ' ' || *s == '\t' || *s == '\n')
                s++;
            if (*s == 0)
                break;
            while (*s && *s != ' ' && *s != '\t' && *s != '\n')
                *op++ = *s++;
            *op = 0;
            matched++;
            fmt++;
            continue;
        }

        {
            char tmp[64];
            int k = 0;
            char *ep;
            while (*s == ' ' || *s == '\t' || *s == '\n')
                s++;
            while (*s && *s != ' ' && *s != '\t' && *s != '\n' && k < 63)
                tmp[k++] = *s++;
            tmp[k] = 0;
            if (k == 0)
                break;
            if (*fmt == 'f') {
                double *dp = va_arg(ap, double *);
                *dp = strtod(tmp, &ep);
                if (ep == tmp)
                    break;
            } else {
                int base = (*fmt == 'x') ? 16 : 10;
                int *ip = va_arg(ap, int *);
                *ip = (int)strtol(tmp, &ep, base);
                if (ep == tmp)
                    break;
                if (*fmt == 'i' && tmp[0] == '0' && (tmp[1] == 'x' || tmp[1] == 'X'))
                    *ip = (int)strtol(tmp, 0, 16);
            }
            matched++;
            fmt++;
        }
    }
    va_end(ap);
    return matched;
}

double __mingw_strtod(const char *nptr, char **endptr)
{
    return strtod(nptr, endptr);
}

/* ---- stdio file ops (no files: ordered to fail) ---------------------- */
void *fopen(const char *path, const char *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}
void *(*__imp_fopen)(const char *, const char *) = fopen;

int fclose(void *stream)
{
    (void)stream;
    return -1;      /* EOF */
}

int fflush(void *stream)
{
    (void)stream;
    return 0;
}

int ferror(void *stream)
{
    (void)stream;
    return 0;
}

int fputs(const char *s, void *stream)
{
    (void)s;
    (void)stream;
    return -1;      /* EOF */
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, void *stream)
{
    (void)ptr;
    (void)size;
    (void)nmemb;
    (void)stream;
    return 0;
}

static FILE ow_iob[3];
void *__acrt_iob_func(unsigned idx)
{
    return idx < 3u ? (void *)&ow_iob[idx] : (void *)&ow_iob[0];
}
void *(*__imp___acrt_iob_func)(unsigned) = __acrt_iob_func;

/* ---- locale (C locale: nothing localised) ---------------------------- */
struct ow_lconv {
    char *decimal_point;
    char *thousands_sep;
    char *grouping;
    char *int_curr_symbol;
    char *currency_symbol;
    char *mon_decimal_point;
    char *mon_thousands_sep;
    char *mon_grouping;
    char *positive_sign;
    char *negative_sign;
    char int_frac_digits;
    char frac_digits;
    char p_cs_precedes;
    char p_sep_by_space;
    char n_cs_precedes;
    char n_sep_by_space;
    char p_sign_posn;
    char n_sign_posn;
    char int_p_cs_precedes;
    char int_p_sep_by_space;
    char int_n_cs_precedes;
    char int_n_sep_by_space;
    char int_p_sign_posn;
    char int_n_sign_posn;
};
static struct ow_lconv ow_c_locale = {
    ".", "", "", "", "", ".", "", "", "", "",
    127, 127, 127, 127, 127, 127, 127, 127,
    127, 127, 127, 127, 127, 127
};
struct ow_lconv *localeconv(void) { return &ow_c_locale; }
struct ow_lconv *(*__imp_localeconv)(void) = localeconv;

/* ---- environment (module has none) ----------------------------------- */
char *getenv(const char *name)
{
    (void)name;
    return NULL;
}

/* ---- rand (deterministic LCG, seeded once) --------------------------- */
static unsigned long long ow_rand_state = UINT64_C(0x0123456789ABCDEF);
int rand(void)
{
    ow_rand_state = ow_rand_state * UINT64_C(6364136223846793005) +
                    UINT64_C(1442695040888963407);
    return (int)((ow_rand_state >> 33) & UINT64_C(0x7FFFFFFF));
}
void srand(unsigned int seed)
{
    ow_rand_state = seed ? (unsigned long long)seed : 1u;
}

/* ---- FILE descriptor shims (no OS files) ----------------------------- */
int _close(int fd)
{
    (void)fd;
    return -1;
}
int (*__imp__close)(int) = _close;

void *_fdopen(int fd, const char *mode)
{
    (void)fd;
    (void)mode;
    return NULL;
}
void *(*__imp__fdopen)(int, const char *) = _fdopen;

void *fdopen(int fd, const char *mode)
{
    return _fdopen(fd, mode);
}
void *(*__imp_fdopen)(int, const char *) = fdopen;

int _open_osfhandle(long long h, int flags)
{
    (void)h;
    (void)flags;
    return -1;
}
int (*__imp__open_osfhandle)(long long, int) = _open_osfhandle;

void *_wfopen(const unsigned short *path, const unsigned short *mode)
{
    (void)path;
    (void)mode;
    return NULL;
}
void *(*__imp__wfopen)(const unsigned short *, const unsigned short *) = _wfopen;

/* ---- Win32 (module has no OS: all handle ops fail) ------------------- */
void *CreateFileW(const void *name, unsigned int access, unsigned int share,
                  void *sa, unsigned int disp, unsigned int flags,
                  void *tpl)
{
    (void)name; (void)access; (void)share; (void)sa; (void)disp;
    (void)flags; (void)tpl;
    return (void *)(long long)-1;   /* INVALID_HANDLE_VALUE */
}
void *(*__imp_CreateFileW)(const void *, unsigned int, unsigned int,
                           void *, unsigned int, unsigned int, void *) =
    CreateFileW;

int CloseHandle(void *h)
{
    (void)h;
    return 0;       /* FALSE */
}
int (*__imp_CloseHandle)(void *) = CloseHandle;

int DeleteFileW(const void *p)
{
    (void)p;
    return 0;       /* FALSE */
}
int (*__imp_DeleteFileW)(const void *) = DeleteFileW;

unsigned int GetTempPathW(unsigned int n, void *buf)
{
    (void)n;
    (void)buf;
    return 0;       /* failure */
}
unsigned int (*__imp_GetTempPathW)(unsigned int, void *) = GetTempPathW;

unsigned int GetTempFileNameW(const void *dir, const void *prefix,
                              unsigned int u, void *name)
{
    (void)dir; (void)prefix; (void)u; (void)name;
    return 0;       /* failure */
}
unsigned int (*__imp_GetTempFileNameW)(const void *, const void *,
                                       unsigned int, void *) =
    GetTempFileNameW;

int QueryPerformanceCounter(void *lp)
{
    (void)lp;
    return 0;       /* FALSE */
}
int (*__imp_QueryPerformanceCounter)(void *) = QueryPerformanceCounter;

int QueryPerformanceFrequency(void *lp)
{
    (void)lp;
    return 0;       /* FALSE */
}
int (*__imp_QueryPerformanceFrequency)(void *) = QueryPerformanceFrequency;