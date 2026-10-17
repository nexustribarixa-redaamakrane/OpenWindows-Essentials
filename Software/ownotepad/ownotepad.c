/*
 * ownotepad.c - OpenWindows Simple Notepad (.owx)
 *
 * A small text editor whose document is a fixed static buffer.  It supports
 * insertion, newline, backspace, line deletion, and cursor motion, and it
 * renders the buffer with line numbers and a caret.  There is no write syscall
 * from Ring 3, so `save` models the write and reports the resulting statistics
 * and checksum.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWNOTEPAD_VERSION "1.0.0"
#define OWNOTEPAD_MAX_BYTES 4096u

static char     s_buf[OWNOTEPAD_MAX_BYTES];
static uint32_t s_len;
static uint32_t s_cursor;
static bool     s_dirty;

/* ── Forward declarations ─────────────────────────────────────── */

static int  ownotepad_run(int argc, const char *const *argv);
static void ownotepad_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        ownotepad_usage();
        return 1;
    }
    return ownotepad_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool ownotepad_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static uint32_t ownotepad_line_start(uint32_t pos)
{
    while (pos > 0u && s_buf[pos - 1u] != '\n') {
        pos--;
    }
    return pos;
}

static uint32_t ownotepad_line_end(uint32_t pos)
{
    while (pos < s_len && s_buf[pos] != '\n') {
        pos++;
    }
    return pos;
}

static void ownotepad_insert_char(char c)
{
    uint32_t i;
    if (s_len >= OWNOTEPAD_MAX_BYTES) {
        return;
    }
    for (i = s_len; i > s_cursor; i--) {
        s_buf[i] = s_buf[i - 1u];
    }
    s_buf[s_cursor] = c;
    s_cursor++;
    s_len++;
    s_dirty = true;
}

static void ownotepad_insert_str(const char *s)
{
    while (*s != '\0') {
        if (s_len >= OWNOTEPAD_MAX_BYTES) {
            break;
        }
        ownotepad_insert_char(*s++);
    }
}

static void ownotepad_backspace(void)
{
    uint32_t i;
    if (s_cursor == 0u) {
        return;
    }
    for (i = s_cursor - 1u; i + 1u < s_len; i++) {
        s_buf[i] = s_buf[i + 1u];
    }
    s_cursor--;
    s_len--;
    s_dirty = true;
}

static void ownotepad_delete_line(void)
{
    uint32_t start = ownotepad_line_start(s_cursor);
    uint32_t end = ownotepad_line_end(s_cursor);
    uint32_t i;
    if (end < s_len && s_buf[end] == '\n') {
        end++;
    }
    for (i = start; end + (i - start) < s_len; i++) {
        s_buf[i] = s_buf[end + (i - start)];
    }
    s_len -= (end - start);
    if (s_cursor > start) {
        s_cursor = start;
    }
    if (s_cursor > s_len) {
        s_cursor = s_len;
    }
    s_dirty = true;
}

static uint32_t ownotepad_checksum(void)
{
    uint32_t sum = 0u;
    uint32_t i;
    for (i = 0u; i < s_len; i++) {
        sum = (sum * 31u + (uint32_t)(uint8_t)s_buf[i]) & 0xFFFFFFFFu;
    }
    return sum;
}

static void ownotepad_put_hex8(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";
    int shift;
    owx_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        owx_putc(digits[(v >> shift) & 0xFu]);
    }
}

static uint32_t ownotepad_count_lines(void)
{
    uint32_t lines = 1u;
    uint32_t i;
    for (i = 0u; i < s_len; i++) {
        if (s_buf[i] == '\n') {
            lines++;
        }
    }
    return lines;
}

/* 1-based row of the cursor, counted from the newlines before its line start. */
static uint32_t ownotepad_cursor_row(void)
{
    uint32_t ls = ownotepad_line_start(s_cursor);
    uint32_t row = 1u;
    uint32_t i;
    for (i = 0u; i < ls; i++) {
        if (s_buf[i] == '\n') {
            row++;
        }
    }
    return row;
}

static void ownotepad_render(void)
{
    uint32_t ls = ownotepad_line_start(s_cursor);
    uint32_t line = 1u;
    uint32_t row_start = 0u;
    uint32_t i;

    owx_puts("ownotepad: ");
    owx_put_u64((uint64_t)ownotepad_count_lines());
    owx_puts(" line(s), ");
    owx_put_u64((uint64_t)s_len);
    owx_puts(" byte(s), dirty=");
    owx_puts(s_dirty ? "yes" : "no");
    owx_puts(", cursor=");
    owx_put_u64((uint64_t)ownotepad_cursor_row());
    owx_putc(':');
    owx_put_u64((uint64_t)(s_cursor - ls + 1u));
    owx_puts("\r\n");

    if (s_len == 0u) {
        owx_puts("  1 | (empty)\r\n");
        owx_puts("    | ^\r\n");
        return;
    }

    for (i = 0u; i <= s_len; i++) {
        if (i == s_len || s_buf[i] == '\n') {
            uint32_t j;
            owx_puts("  ");
            owx_put_u64((uint64_t)line);
            owx_puts(" | ");
            for (j = row_start; j < i; j++) {
                owx_putc(s_buf[j]);
            }
            owx_puts("\r\n");
            if (s_cursor >= row_start && s_cursor <= i) {
                uint32_t col = s_cursor - row_start;
                uint32_t k;
                owx_puts("    | ");
                for (k = 0u; k < col; k++) {
                    owx_putc(' ');
                }
                owx_puts("^\r\n");
            }
            line++;
            row_start = i + 1u;
        }
    }
}

/* ── Implementation ───────────────────────────────────────────── */

static int ownotepad_run(int argc, const char *const *argv)
{
    int i;
    int bad = 0;

    s_len = 0u;
    s_cursor = 0u;
    s_dirty = false;

    if (argc <= 1) {
        owx_puts("ownotepad: empty buffer; use commands such as "
                 "ins <word> sp nl bs del text save list\r\n");
        ownotepad_render();
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const char *cmd = argv[i];
        if (cmd == NULL) {
            continue;
        }
        if (ownotepad_streq(cmd, "ins") || ownotepad_streq(cmd, "type")) {
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                owx_puts("ownotepad: ins requires a word\r\n");
                bad = 1;
                continue;
            }
            ownotepad_insert_str(argv[++i]);
        } else if (ownotepad_streq(cmd, "text")) {
            int j;
            bool first = true;
            if (i + 1 >= argc) {
                owx_puts("ownotepad: text requires text\r\n");
                bad = 1;
                continue;
            }
            for (j = i + 1; j < argc; j++) {
                if (argv[j] == NULL) {
                    continue;
                }
                if (!first) {
                    ownotepad_insert_char(' ');
                }
                ownotepad_insert_str(argv[j]);
                first = false;
            }
            i = argc - 1;
        } else if (ownotepad_streq(cmd, "sp")) {
            ownotepad_insert_char(' ');
        } else if (ownotepad_streq(cmd, "nl")) {
            ownotepad_insert_char('\n');
        } else if (ownotepad_streq(cmd, "bs")) {
            ownotepad_backspace();
        } else if (ownotepad_streq(cmd, "del")) {
            ownotepad_delete_line();
        } else if (ownotepad_streq(cmd, "left")) {
            if (s_cursor > 0u) {
                s_cursor--;
            }
        } else if (ownotepad_streq(cmd, "right")) {
            if (s_cursor < s_len) {
                s_cursor++;
            }
        } else if (ownotepad_streq(cmd, "home")) {
            s_cursor = ownotepad_line_start(s_cursor);
        } else if (ownotepad_streq(cmd, "end")) {
            s_cursor = ownotepad_line_end(s_cursor);
        } else if (ownotepad_streq(cmd, "clear")) {
            s_len = 0u;
            s_cursor = 0u;
            s_dirty = false;
        } else if (ownotepad_streq(cmd, "save")) {
            owx_puts("ownotepad: save -> ");
            owx_put_u64((uint64_t)s_len);
            owx_puts(" byte(s) in ");
            owx_put_u64((uint64_t)ownotepad_count_lines());
            owx_puts(" line(s), checksum=");
            ownotepad_put_hex8(ownotepad_checksum());
            owx_puts(" (modelled: no Ring 3 write syscall)\r\n");
            s_dirty = false;
            continue;
        } else if (ownotepad_streq(cmd, "-h") || ownotepad_streq(cmd, "--help")) {
            ownotepad_usage();
            return 0;
        } else if (ownotepad_streq(cmd, "list") || ownotepad_streq(cmd, "show")) {
            ownotepad_render();
            continue;
        } else {
            owx_puts("ownotepad: unknown command '");
            owx_puts(cmd);
            owx_puts("'\r\n");
            ownotepad_usage();
            return 2;
        }

        ownotepad_render();
    }

    return bad;
}

static void ownotepad_usage(void)
{
    owx_puts(
        "usage: ownotepad [command ...]\r\n"
        "  ins <word>        insert one word at the cursor\r\n"
        "  sp                insert a single space\r\n"
        "  text <...>        insert the remaining words, space separated\r\n"
        "  nl                insert a newline\r\n"
        "  bs                delete the character before the cursor\r\n"
        "  del               delete the current line\r\n"
        "  left | right      move the cursor by one character\r\n"
        "  home | end        move the cursor to the line start / end\r\n"
        "  save              model a save and report size, lines, checksum\r\n"
        "  list              print the buffer with a caret\r\n"
        "  clear             empty the buffer\r\n");
}
