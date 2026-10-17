/*
 * owterm.c - OpenWindows Terminal Emulator (.owx)
 *
 * A headless terminal emulator: a rows-by-cols character screen, a cursor, and
 * a real byte processor for printable text, CR/LF, backspace, tab, and the
 * common ANSI escape sequences (erase-display, erase-line, cursor motion and
 * positioning).  Input can be fed from the argument vector or typed at the
 * console, and the resulting screen is rendered as text.  Echo of typed input
 * is togglable.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWTERM_VERSION "1.0.0"
#define OWTERM_MAX_COLS 96u
#define OWTERM_MAX_ROWS 32u
#define OWTERM_INPUT_MAX 512u

static char     s_screen[OWTERM_MAX_ROWS][OWTERM_MAX_COLS + 1u];
static uint32_t s_cols;
static uint32_t s_rows;
static uint32_t s_row;
static uint32_t s_col;

/* ANSI parser state. */
static int      s_esc;              /* 0 text, 1 ESC, 2 CSI, 3 OSC, 4 charset */
static int32_t  s_params[8];
static uint32_t s_nparams;
static int32_t  s_cur;
static bool     s_have_param;
static bool     s_private;

static const char owterm_demo[] =
    "\x1B[2J\x1B[H"
    "owterm: OpenWindows terminal model\r\n"
    "printable text wraps at the right margin\r\n"
    "backspace: abc\b\b\bXYZ\r\n"
    "erase-line: 123456789\x1B[4D\x1B[K\r\n"
    "cursor: \x1B[6;12H* placed with CSI row;col H\r\n";

/* ── Forward declarations ─────────────────────────────────────── */

static int  owterm_run(int argc, const char *const *argv);
static void owterm_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owterm_usage();
        return 1;
    }
    return owterm_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owterm_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool owterm_parse_u32(const char *s, uint32_t *out)
{
    uint64_t v = 0u;
    if (s == NULL || *s == '\0') {
        return false;
    }
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        v = v * 10u + (uint64_t)(uint32_t)(*s - '0');
        if (v > 1000u) {
            return false;
        }
        s++;
    }
    *out = (uint32_t)v;
    return true;
}

static void owterm_clear_screen(void)
{
    uint32_t r;
    uint32_t c;
    for (r = 0u; r < s_rows; r++) {
        for (c = 0u; c < s_cols; c++) {
            s_screen[r][c] = ' ';
        }
        s_screen[r][s_cols] = '\0';
    }
    s_row = 0u;
    s_col = 0u;
}

/* Scroll every line up by one and blank the last. */
static void owterm_scroll(void)
{
    uint32_t r;
    uint32_t c;
    for (r = 0u; r + 1u < s_rows; r++) {
        for (c = 0u; c < s_cols; c++) {
            s_screen[r][c] = s_screen[r + 1u][c];
        }
    }
    for (c = 0u; c < s_cols; c++) {
        s_screen[s_rows - 1u][c] = ' ';
    }
    if (s_row > 0u) {
        s_row = s_rows - 1u;
    }
}

static void owterm_newline(void)
{
    s_row++;
    if (s_row >= s_rows) {
        owterm_scroll();
    }
}

static void owterm_put_char(char ch)
{
    if (ch == '\n') {
        owterm_newline();
        return;
    }
    if (ch == '\r') {
        s_col = 0u;
        return;
    }
    if (ch == '\b') {
        if (s_col > 0u) {
            s_col--;
        }
        return;
    }
    if (ch == '\t') {
        s_col = (s_col + 8u) & ~7u;
        if (s_col >= s_cols) {
            s_col = 0u;
            owterm_newline();
        }
        return;
    }
    if ((unsigned char)ch < 32u || (unsigned char)ch == 127u) {
        return;
    }
    s_screen[s_row][s_col] = ch;
    s_col++;
    if (s_col >= s_cols) {
        s_col = 0u;
        owterm_newline();
    }
}

static int32_t owterm_param(uint32_t i, int32_t def)
{
    if (i >= s_nparams) {
        return def;
    }
    return s_params[i];
}

static void owterm_erase_line(int mode)
{
    uint32_t c;
    if (mode == 2) {
        for (c = 0u; c < s_cols; c++) {
            s_screen[s_row][c] = ' ';
        }
    } else if (mode == 1) {
        for (c = 0u; c <= s_col && c < s_cols; c++) {
            s_screen[s_row][c] = ' ';
        }
    } else {
        for (c = s_col; c < s_cols; c++) {
            s_screen[s_row][c] = ' ';
        }
    }
}

static void owterm_erase_display(int mode)
{
    uint32_t r;
    uint32_t c;
    if (mode == 2 || mode == 3) {
        owterm_clear_screen();
        return;
    }
    if (mode == 0) {
        owterm_erase_line(0);
        for (r = s_row + 1u; r < s_rows; r++) {
            for (c = 0u; c < s_cols; c++) {
                s_screen[r][c] = ' ';
            }
        }
    } else if (mode == 1) {
        owterm_erase_line(1);
        for (r = 0u; r < s_row; r++) {
            for (c = 0u; c < s_cols; c++) {
                s_screen[r][c] = ' ';
            }
        }
    }
}

static void owterm_csi_dispatch(char final)
{
    int32_t n = owterm_param(0u, 1);
    switch (final) {
    case 'A':
        s_row = (n > (int32_t)s_row) ? 0u : (uint32_t)((int32_t)s_row - n);
        break;
    case 'B':
        s_row += (uint32_t)((n < 1) ? 1 : n);
        if (s_row >= s_rows) {
            s_row = s_rows - 1u;
        }
        break;
    case 'C':
        s_col += (uint32_t)((n < 1) ? 1 : n);
        if (s_col >= s_cols) {
            s_col = s_cols - 1u;
        }
        break;
    case 'D':
        s_col = (n > (int32_t)s_col) ? 0u : (uint32_t)((int32_t)s_col - n);
        break;
    case 'H':
    case 'f': {
        int32_t row = owterm_param(0u, 1);
        int32_t col = owterm_param(1u, 1);
        if (row < 1) {
            row = 1;
        }
        if (col < 1) {
            col = 1;
        }
        s_row = ((uint32_t)row - 1u < s_rows) ? (uint32_t)(row - 1) : s_rows - 1u;
        s_col = ((uint32_t)col - 1u < s_cols) ? (uint32_t)(col - 1) : s_cols - 1u;
        break;
    }
    case 'J':
        owterm_erase_display(owterm_param(0u, 0));
        break;
    case 'K':
        owterm_erase_line(owterm_param(0u, 0));
        break;
    default:
        (void)s_private;    /* SGR and other finals are ignored in the model */
        break;
    }
}

/* Process one input byte through the ANSI state machine. */
static void owterm_feed_byte(int byte)
{
    char c = (char)(unsigned char)byte;
    if (s_esc == 0) {
        if (byte == 0x1B) {
            s_esc = 1;
        } else {
            owterm_put_char(c);
        }
        return;
    }
    if (s_esc == 1) {
        if (c == '[') {
            s_esc = 2;
            s_nparams = 0u;
            s_cur = 0;
            s_have_param = false;
            s_private = false;
        } else if (c == ']') {
            s_esc = 3;
        } else if (c == '(' || c == ')') {
            s_esc = 4;
        } else {
            s_esc = 0;
        }
        return;
    }
    if (s_esc == 2) {
        if (c >= '0' && c <= '9') {
            s_cur = s_cur * 10 + (int32_t)(c - '0');
            s_have_param = true;
            return;
        }
        if (c == ';') {
            if (s_nparams < 8u) {
                s_params[s_nparams++] = s_have_param ? s_cur : 0;
            }
            s_cur = 0;
            s_have_param = false;
            return;
        }
        if (c == '?') {
            s_private = true;
            return;
        }
        if (s_nparams < 8u) {
            s_params[s_nparams++] = s_have_param ? s_cur : 0;
        }
        owterm_csi_dispatch(c);
        s_esc = 0;
        return;
    }
    if (s_esc == 3) {
        if (byte == 7 || byte == 0x1B) {
            s_esc = 0;
        }
        return;
    }
    /* s_esc == 4: consume the one charset-designator byte. */
    s_esc = 0;
}

static void owterm_feed(const char *s)
{
    while (*s != '\0') {
        owterm_feed_byte((int)(unsigned char)*s);
        s++;
    }
}

static void owterm_render(void)
{
    uint32_t c;
    uint32_t r;
    owx_puts("+");
    for (c = 0u; c < s_cols; c++) {
        owx_putc('-');
    }
    owx_puts("+\r\n");
    for (r = 0u; r < s_rows; r++) {
        owx_putc('|');
        owx_puts(s_screen[r]);
        owx_puts("|\r\n");
    }
    owx_puts("+");
    for (c = 0u; c < s_cols; c++) {
        owx_putc('-');
    }
    owx_puts("+\r\n");
    owx_puts("owterm: ");
    owx_put_u64((uint64_t)s_rows);
    owx_putc('x');
    owx_put_u64((uint64_t)s_cols);
    owx_puts(" cursor=");
    owx_put_u64((uint64_t)(s_row + 1u));
    owx_putc(':');
    owx_put_u64((uint64_t)(s_col + 1u));
    owx_puts("\r\n");
}

/* ── Implementation ───────────────────────────────────────────── */

static int owterm_run(int argc, const char *const *argv)
{
    bool interactive = false;
    bool echo = false;
    bool echo_set = false;
    char input[OWTERM_INPUT_MAX];
    uint32_t inlen = 0u;
    int i;

    s_cols = 64u;
    s_rows = 16u;

    for (i = 1; i < argc; i++) {
        const char *opt = argv[i];
        if (opt == NULL) {
            continue;
        }
        if (owterm_streq(opt, "-c") || owterm_streq(opt, "--cols")) {
            if (!owterm_parse_u32((i + 1 < argc) ? argv[i + 1] : NULL, &s_cols) ||
                s_cols < 8u || s_cols > OWTERM_MAX_COLS) {
                owx_puts("owterm: -c must be 8..96\r\n");
                return 2;
            }
            i++;
        } else if (owterm_streq(opt, "-r") || owterm_streq(opt, "--rows")) {
            if (!owterm_parse_u32((i + 1 < argc) ? argv[i + 1] : NULL, &s_rows) ||
                s_rows < 4u || s_rows > OWTERM_MAX_ROWS) {
                owx_puts("owterm: -r must be 4..32\r\n");
                return 2;
            }
            i++;
        } else if (owterm_streq(opt, "-t") || owterm_streq(opt, "--text")) {
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                owx_puts("owterm: -t requires text\r\n");
                return 2;
            }
            i++;
            {
                const char *p = argv[i];
                while (*p != '\0' && inlen + 1u < OWTERM_INPUT_MAX) {
                    input[inlen++] = *p++;
                }
            }
        } else if (owterm_streq(opt, "-i") || owterm_streq(opt, "--interactive")) {
            interactive = true;
        } else if (owterm_streq(opt, "--echo")) {
            echo = true;
            echo_set = true;
        } else if (owterm_streq(opt, "--no-echo")) {
            echo = false;
            echo_set = true;
        } else if (owterm_streq(opt, "-h") || owterm_streq(opt, "--help")) {
            owterm_usage();
            return 0;
        } else if (opt[0] == '-') {
            owx_puts("owterm: unknown option '");
            owx_puts(opt);
            owx_puts("'\r\n");
            owterm_usage();
            return 2;
        } else {
            /* Positional arguments are appended to the input stream. */
            const char *p = opt;
            if (inlen > 0u && inlen + 1u < OWTERM_INPUT_MAX) {
                input[inlen++] = ' ';
            }
            while (*p != '\0' && inlen + 1u < OWTERM_INPUT_MAX) {
                input[inlen++] = *p++;
            }
        }
    }
    input[inlen] = '\0';

    owterm_clear_screen();

    if (interactive) {
        if (!echo_set) {
            echo = true;    /* local echo by default for a typed session */
        }
        owx_puts("owterm: interactive; Ctrl-D ends the session\r\n");
        for (;;) {
            int c = owx_getc_wait();
            if (c < 0 || c == 4) {
                break;
            }
            if (echo) {
                owx_putc((char)c);
            }
            owterm_feed_byte(c);
        }
        if (echo) {
            owx_puts("\r\n");
        }
    } else if (inlen > 0u) {
        input[inlen] = '\0';
        owterm_feed(input);
    } else {
        owterm_feed(owterm_demo);
    }

    owterm_render();
    return 0;
}

static void owterm_usage(void)
{
    owx_puts(
        "usage: owterm [-c cols] [-r rows] [-t text] [-i] [--echo|--no-echo] [-h]\r\n"
        "  -c, --cols N      screen width, 8..96 (default 64)\r\n"
        "  -r, --rows N      screen height, 4..32 (default 16)\r\n"
        "  -t, --text S      feed S as terminal input (repeatable)\r\n"
        "  text ...          positional arguments are fed, space separated\r\n"
        "  -i, --interactive read input from the console until Ctrl-D\r\n"
        "  --echo/--no-echo  local echo of typed input (default: on with -i)\r\n"
        "  With no input the built-in demo sequence is processed.\r\n");
}
