/*
 * owcalcgui.c - OpenWindows GUI Calculator (.owx)
 *
 * A headless model of the graphical calculator.  It keeps a display string and
 * a fixed-point (milli-unit) recursive-descent expression evaluator, and it
 * accepts button presses as arguments - digits, decimal point, operators,
 * parentheses, C/CE/back/neg/= - printing the display after each press.  The
 * grammar is the usual one: expr := term (('+'|'-') term)*, term := factor
 * (('*'|'/') factor)*, factor := ('+'|'-') primary, primary := number | '(' expr
 * ')'.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWCALCGUI_VERSION "1.0.0"
#define OWCALCGUI_DISPLAY_MAX 64u
#define OWCALCGUI_SCALE 1000        /* thousandths */

static char s_display[OWCALCGUI_DISPLAY_MAX];
static uint32_t s_dlen;
static bool s_error;

/* ── Forward declarations ─────────────────────────────────────── */

static int  owcalcgui_run(int argc, const char *const *argv);
static void owcalcgui_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owcalcgui_usage();
        return 1;
    }
    return owcalcgui_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owcalcgui_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static int64_t owcalcgui_pow10(int n)
{
    int64_t v = 1;
    while (n > 0) {
        v *= 10;
        n--;
    }
    return v;
}

static void owcalcgui_skip(const char **p)
{
    while (**p == ' ' || **p == '\t') {
        (*p)++;
    }
}

static bool owcalcgui_primary(const char **p, int64_t *out);
static bool owcalcgui_expr(const char **p, int64_t *out);

static bool owcalcgui_number(const char **p, int64_t *out)
{
    int64_t intpart = 0;
    int64_t frac = 0;
    int fracdigits = 0;
    bool any = false;

    while (**p >= '0' && **p <= '9') {
        intpart = intpart * 10 + (int64_t)(**p - '0');
        (*p)++;
        any = true;
    }
    if (**p == '.') {
        (*p)++;
        while (**p >= '0' && **p <= '9') {
            if (fracdigits < 3) {
                frac = frac * 10 + (int64_t)(**p - '0');
                fracdigits++;
            }
            (*p)++;
            any = true;
        }
    }
    if (!any) {
        return false;
    }
    *out = intpart * OWCALCGUI_SCALE +
           frac * owcalcgui_pow10(3 - fracdigits);
    return true;
}

static bool owcalcgui_primary(const char **p, int64_t *out)
{
    owcalcgui_skip(p);
    if (**p == '(') {
        (*p)++;
        if (!owcalcgui_expr(p, out)) {
            return false;
        }
        owcalcgui_skip(p);
        if (**p != ')') {
            return false;
        }
        (*p)++;
        return true;
    }
    return owcalcgui_number(p, out);
}

static bool owcalcgui_factor(const char **p, int64_t *out)
{
    owcalcgui_skip(p);
    if (**p == '+') {
        (*p)++;
        return owcalcgui_factor(p, out);
    }
    if (**p == '-') {
        (*p)++;
        if (!owcalcgui_factor(p, out)) {
            return false;
        }
        *out = -*out;
        return true;
    }
    return owcalcgui_primary(p, out);
}

static bool owcalcgui_term(const char **p, int64_t *out)
{
    int64_t value;
    if (!owcalcgui_factor(p, &value)) {
        return false;
    }
    for (;;) {
        owcalcgui_skip(p);
        if (**p == '*') {
            int64_t rhs;
            (*p)++;
            if (!owcalcgui_factor(p, &rhs)) {
                return false;
            }
            value = (value * rhs) / OWCALCGUI_SCALE;
        } else if (**p == '/') {
            int64_t rhs;
            (*p)++;
            if (!owcalcgui_factor(p, &rhs)) {
                return false;
            }
            if (rhs == 0) {
                return false;
            }
            value = (value * OWCALCGUI_SCALE) / rhs;
        } else {
            break;
        }
    }
    *out = value;
    return true;
}

static bool owcalcgui_expr(const char **p, int64_t *out)
{
    int64_t value;
    if (!owcalcgui_term(p, &value)) {
        return false;
    }
    for (;;) {
        owcalcgui_skip(p);
        if (**p == '+') {
            int64_t rhs;
            (*p)++;
            if (!owcalcgui_term(p, &rhs)) {
                return false;
            }
            value += rhs;
        } else if (**p == '-') {
            int64_t rhs;
            (*p)++;
            if (!owcalcgui_term(p, &rhs)) {
                return false;
            }
            value -= rhs;
        } else {
            break;
        }
    }
    *out = value;
    return true;
}

/* Evaluate a full expression; false on syntax error or division by zero. */
static bool owcalcgui_eval(const char *s, int64_t *out)
{
    const char *p = s;
    int64_t v;
    if (!owcalcgui_expr(&p, &v)) {
        return false;
    }
    owcalcgui_skip(&p);
    if (*p != '\0') {
        return false;
    }
    *out = v;
    return true;
}

/* Render a milli-unit value with up to three trimmed decimals. */
static void owcalcgui_format(int64_t v, char *buf, uint32_t cap)
{
    uint32_t n = 0u;
    bool neg = (v < 0);
    uint64_t a = neg ? ((uint64_t)0u - (uint64_t)v) : (uint64_t)v;
    uint64_t ip = a / (uint64_t)OWCALCGUI_SCALE;
    uint64_t fp = a % (uint64_t)OWCALCGUI_SCALE;
    char tmp[24];
    uint32_t t = 0u;
    uint32_t i;

    if (neg && n + 1u < cap) {
        buf[n++] = '-';
    }
    if (ip == 0u) {
        tmp[t++] = '0';
    }
    while (ip > 0u && t < sizeof(tmp)) {
        tmp[t++] = (char)('0' + (int)(ip % 10u));
        ip /= 10u;
    }
    for (i = 0u; i < t && n + 1u < cap; i++) {
        buf[n++] = tmp[t - 1u - i];
    }
    if (fp != 0u && n + 1u < cap) {
        char frac[4];
        int k;
        buf[n++] = '.';
        for (k = 2; k >= 0; k--) {
            frac[k] = (char)('0' + (int)(fp % 10u));
            fp /= 10u;
        }
        k = 2;
        while (k >= 0 && frac[k] == '0') {
            k--;
        }
        for (i = 0u; i <= (uint32_t)k && n + 1u < cap; i++) {
            buf[n++] = frac[i];
        }
    }
    buf[n] = '\0';
}

static void owcalcgui_set(const char *s)
{
    uint32_t i = 0u;
    while (s[i] != '\0' && i + 1u < OWCALCGUI_DISPLAY_MAX) {
        s_display[i] = s[i];
        i++;
    }
    s_display[i] = '\0';
    s_dlen = i;
}

static void owcalcgui_append(char c)
{
    if (s_dlen + 1u < OWCALCGUI_DISPLAY_MAX) {
        s_display[s_dlen++] = c;
        s_display[s_dlen] = '\0';
    }
}

static void owcalcgui_backspace(void)
{
    if (s_dlen > 0u) {
        s_dlen--;
        s_display[s_dlen] = '\0';
    }
}

static void owcalcgui_show(void)
{
    owx_puts("owcalcgui: display = ");
    owx_puts(s_dlen > 0u ? s_display : "0");
    if (s_error) {
        owx_puts("  [error]");
    }
    owx_puts("\r\n");
}

/* Evaluate what is on the display and replace it with the result. */
static void owcalcgui_equals(void)
{
    int64_t v = 0;
    if (s_error || s_dlen == 0u) {
        s_error = true;
        owcalcgui_set("E");
        return;
    }
    if (owcalcgui_eval(s_display, &v)) {
        owcalcgui_format(v, s_display, OWCALCGUI_DISPLAY_MAX);
        s_dlen = 0u;
        while (s_display[s_dlen] != '\0') {
            s_dlen++;
        }
        s_error = false;
    } else {
        owcalcgui_set("E");
        s_error = true;
    }
}

/* Toggle the sign of the trailing numeric literal. */
static void owcalcgui_negate(void)
{
    uint32_t start = s_dlen;
    uint32_t i;
    if (s_error) {
        return;
    }
    while (start > 0u) {
        char c = s_display[start - 1u];
        if ((c >= '0' && c <= '9') || c == '.') {
            start--;
        } else {
            break;
        }
    }
    if (start == s_dlen) {
        return;
    }
    if (start > 0u && s_display[start - 1u] == '-') {
        bool unary = (start == 1u) || s_display[start - 2u] == '(' ||
                     s_display[start - 2u] == '+' || s_display[start - 2u] == '-' ||
                     s_display[start - 2u] == '*' || s_display[start - 2u] == '/';
        if (unary) {
            for (i = start - 1u; i + 1u < s_dlen; i++) {
                s_display[i] = s_display[i + 1u];
            }
            s_dlen--;
            s_display[s_dlen] = '\0';
            return;
        }
    }
    if (s_dlen + 1u < OWCALCGUI_DISPLAY_MAX) {
        for (i = s_dlen; i > start; i--) {
            s_display[i] = s_display[i - 1u];
        }
        s_display[start] = '-';
        s_dlen++;
        s_display[s_dlen] = '\0';
    }
}

/* Apply one button.  Returns false for an unknown button. */
static bool owcalcgui_button(const char *b)
{
    char c;

    /* Named buttons are matched first, so single-character names such as "C"
     * and "=" are not mistaken for a digit/operator key. */
    if (owcalcgui_streq(b, "C") || owcalcgui_streq(b, "clear") ||
        owcalcgui_streq(b, "CE")) {
        owcalcgui_set("");
        s_error = false;
        return true;
    }
    if (owcalcgui_streq(b, "back")) {
        owcalcgui_backspace();
        return true;
    }
    if (owcalcgui_streq(b, "neg") || owcalcgui_streq(b, "+/-")) {
        owcalcgui_negate();
        return true;
    }
    if (owcalcgui_streq(b, "eq") || owcalcgui_streq(b, "=") ||
        owcalcgui_streq(b, "eval")) {
        owcalcgui_equals();
        return true;
    }

    if (b[0] == '\0' || b[1] != '\0') {
        return false;
    }
    c = b[0];
    if ((c >= '0' && c <= '9') || c == '.') {
        if (s_error) {
            owcalcgui_set("");
            s_error = false;
        }
        owcalcgui_append(c);
        return true;
    }
    if (c == '+' || c == '-' || c == '*' || c == '/' || c == '(' || c == ')') {
        if (s_error) {
            owcalcgui_set("");
            s_error = false;
        }
        owcalcgui_append(c);
        return true;
    }
    return false;
}

/* ── Implementation ───────────────────────────────────────────── */

static int owcalcgui_run(int argc, const char *const *argv)
{
    int i;

    owcalcgui_set("");
    s_error = false;

    if (argc <= 1) {
        owx_puts("owcalcgui: ready; buttons: digits . + - * / ( ) = C CE back neg\r\n");
        owcalcgui_show();
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (arg == NULL) {
            continue;
        }
        if (owcalcgui_streq(arg, "-h") || owcalcgui_streq(arg, "--help")) {
            owcalcgui_usage();
            return 0;
        }
        if (owcalcgui_streq(arg, "-e") || owcalcgui_streq(arg, "--expr")) {
            int64_t v = 0;
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                owx_puts("owcalcgui: -e requires an expression\r\n");
                return 2;
            }
            i++;
            if (owcalcgui_eval(argv[i], &v)) {
                owcalcgui_format(v, s_display, OWCALCGUI_DISPLAY_MAX);
                s_dlen = 0u;
                while (s_display[s_dlen] != '\0') {
                    s_dlen++;
                }
                s_error = false;
                owx_puts("owcalcgui: ");
                owx_puts(argv[i]);
                owx_puts(" = ");
                owx_puts(s_display);
                owx_puts("\r\n");
            } else {
                s_error = true;
                owx_puts("owcalcgui: cannot evaluate '");
                owx_puts(argv[i]);
                owx_puts("'\r\n");
            }
            continue;
        }
        if (!owcalcgui_button(arg)) {
            owx_puts("owcalcgui: unknown button '");
            owx_puts(arg);
            owx_puts("'\r\n");
            owcalcgui_usage();
            return 2;
        }
        owcalcgui_show();
    }

    return s_error ? 1 : 0;
}

static void owcalcgui_usage(void)
{
    owx_puts(
        "usage: owcalcgui [button ...]\r\n"
        "  buttons: 0-9 . + - * / ( ) =   press keys onto the display\r\n"
        "  C | CE | clear   clear the display\r\n"
        "  back             delete the last character\r\n"
        "  neg | +/-        toggle the sign of the trailing number\r\n"
        "  eq               evaluate the display (same as '=')\r\n"
        "  -e EXPR          evaluate EXPR once without touching buttons\r\n"
        "  Arithmetic is fixed-point with three decimals.\r\n");
}
