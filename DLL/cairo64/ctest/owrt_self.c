/* owrt_self.c — exercises the freestanding owrt shims in isolation.
 * HOSTED probe linked against the owrt member objects directly (NOT the
 * archive, which must stay out of hosted links so its malloc does not
 * shadow the CRT allocator). Members used: ow_jmp, ow_math, ow_fmt,
 * ow_str, ow_crt. Every function under test is the module's own code.
 */
#include <stddef.h>
#include <stdarg.h>

/* Hosted CRT symbols the probe itself uses. */
int   printf(const char *fmt, ...);
void *__crt_malloc(size_t size);           /* for the memcpy scratch only */

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = dst;
    const unsigned char *s = src;
    size_t i;
    for (i = 0; i < n; ++i)
        d[i] = s[i];
    return dst;
}

/* ---- owrt units under test ---- */
int    _setjmp(void *buf);
void   longjmp(void *buf, int v);
double _hypot(double x, double y);
double strtod(const char *nptr, char **endptr);
int    snprintf(char *buf, size_t n, const char *fmt, ...);
long   strtol(const char *nptr, char **endptr, int base);
int    isdigit(int c);
int    isspace(int c);
int    rand(void);
double hypot(double x, double y);

static int failures = 0;

static void check(const char *what, int ok)
{
    printf("%s: %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        failures++;
}

static int probe_big_stack(void) __attribute__((noinline));
static int probe_big_stack(void)
{
    volatile char pad[50000];
    volatile unsigned long acc = 0;
    unsigned int i;
    for (i = 0; i < sizeof pad; i++)
        pad[i] = (char)(i & 7);
    for (i = 0; i < sizeof pad; i++)
        acc += (unsigned char)pad[i];
    return (int)(acc ^ 0xDEADBEEF);
}

int main(void)
{
    unsigned long long jbuf[13];
    char buf[64];

    if (_setjmp((void *)jbuf) == 0)
        longjmp((void *)jbuf, 42);
    else
        check("setjmp/longjmp resumes with value", 1);

    if (_setjmp((void *)jbuf) == 0)
        longjmp((void *)jbuf, 0);
    else
        check("longjmp(0) resumes as 1", 1);

    {
        int r1 = probe_big_stack();
        int r2 = probe_big_stack();
        check("chkstk big-frame probe", r1 == r2 && r1 != 0);
    }

    check("hypot(3,4)=5", _hypot(3.0, 4.0) == 5.0 && hypot(6.0, 8.0) == 10.0);

    check("strtod", strtod("3.14159", 0) > 3.14 && strtod("3.14159", 0) < 3.15);
    check("strtol(-ff,16)", strtol("-ff", 0, 16) == -255);

    snprintf(buf, sizeof buf, "%d/%s/%05d/%.2f", -7, "owrt", 3, 3.14159);
    check("snprintf", buf[0] == '-');

    check("isdigit('7')", isdigit('7') && !isdigit('a'));
    check("isspace", isspace(' ') && isspace('\t') && !isspace('x'));
    check("rand in range", rand() >= 0);

    printf("owrt self-test %s\n", failures ? "FAILED" : "passed");
    return failures ? 1 : 0;
}