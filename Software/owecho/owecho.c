/*
 * owecho.c - OpenWindows Argument Echo Utility (.owx)
 *
 * Writes its operands to the console separated by single spaces.  Two switches
 * are honoured, both scanned with the same "cluster of letters" rule real
 * `echo` uses:
 *
 *   -n   do not append the trailing newline
 *   -e   interpret backslash escapes in each operand
 *
 * With -e the recognised escapes are \n \t \r \\ \0 and \xHH (one or two hex
 * digits).  An unrecognised escape is copied through verbatim, backslash and
 * all, so `echo -e 'a\qb'` prints `a\qb` rather than silently eating the q.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWECHO_VERSION "1.0.0"

/* ── Forward declarations ─────────────────────────────────────── */

static int  owecho_run(int argc, const char *const *argv);
static void owecho_usage(void);

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owecho_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* One hex digit, or -1.  Only [0-9a-fA-F] are accepted. */
static int owecho_hexval(char c)
{
    if (c >= '0' && c <= '9') return (int)(c - '0');
    if (c >= 'a' && c <= 'f') return (int)(c - 'a') + 10;
    if (c >= 'A' && c <= 'F') return (int)(c - 'A') + 10;
    return -1;
}

/*
 * Emit one operand.  `esc` enables escape processing; when a backslash is the
 * last character it is emitted literally, which is what makes `\` round-trip.
 */
static void owecho_emit(const char *s, bool esc)
{
    while (*s != '\0') {
        char c = *s++;
        if (!esc || c != '\\') {
            owx_putc(c);
            continue;
        }
        char e = *s;
        if (e == '\0') {           /* trailing backslash: keep it */
            owx_putc('\\');
            break;
        }
        s++;
        switch (e) {
        case 'n': owx_putc('\n'); break;
        case 't': owx_putc('\t'); break;
        case 'r': owx_putc('\r'); break;
        case '\\': owx_putc('\\'); break;
        case '0': owx_putc('\0'); break;
        case 'x': {
            int hi = owecho_hexval(*s);
            if (hi < 0) {          /* not a hex escape: literal "\x" */
                owx_putc('\\');
                owx_putc('x');
                break;
            }
            s++;
            int value = hi;
            int lo = owecho_hexval(*s);
            if (lo >= 0) {
                value = (value << 4) | lo;
                s++;
            }
            owx_putc((char)value);
            break;
        }
        default:
            /* Unknown escape: reproduce it exactly. */
            owx_putc('\\');
            owx_putc(e);
            break;
        }
    }
}

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owecho_usage();
        return 1;
    }
    return owecho_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owecho_run(int argc, const char *const *argv)
{
    bool newline = true;
    bool esc = false;
    int i = 1;

    if (argc > 1 && argv != NULL && argv[1] != NULL &&
        (owecho_streq(argv[1], "-h") || owecho_streq(argv[1], "--help"))) {
        owecho_usage();
        return 0;
    }

    /* Option cluster scan, exactly like the shell builtin: letters may be
     * combined ("-ne"), and the first token that is not a valid switch ends
     * option processing. */
    while (i < argc) {
        const char *a = argv[i];
        if (a == NULL || a[0] != '-' || a[1] == '\0') {
            break;
        }
        if (a[1] == '-' && a[2] == '\0') {  /* "--" forces end of options */
            i++;
            break;
        }
        bool okay = true;
        uint32_t k;
        for (k = 1u; a[k] != '\0'; k++) {
            if (a[k] == 'n') {
                newline = false;
            } else if (a[k] == 'e') {
                esc = true;
            } else if (a[k] == 'E') {
                esc = false;
            } else {
                okay = false;
                break;
            }
        }
        if (!okay) {
            break;
        }
        i++;
    }

    bool first = true;
    for (; i < argc; i++) {
        if (argv[i] == NULL) {
            continue;
        }
        if (!first) {
            owx_putc(' ');
        }
        first = false;
        owecho_emit(argv[i], esc);
    }

    if (newline) {
        owx_puts("\r\n");
    }
    return 0;
}

static void owecho_usage(void)
{
    owx_puts("owecho " OWECHO_VERSION " - echo arguments\r\n");
    owx_puts("usage: owecho [-n] [-e] [--] [string...]\r\n");
    owx_puts("  -n        suppress the trailing newline\r\n");
    owx_puts("  -e        interpret \\n \\t \\r \\\\ \\0 and \\xHH escapes\r\n");
}
