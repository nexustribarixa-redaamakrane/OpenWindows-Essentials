/*
 * owpaint.c - OpenWindows Simple Paint Application (.owx)
 *
 * A headless paint model: a fixed 32x16 character canvas with point, line
 * (Bresenham), rectangle (outline or filled), and four-way flood-fill
 * operations.  Drawing commands are taken from the argument vector and the
 * resulting canvas is rendered as ASCII art inside a frame.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWPAINT_VERSION "1.0.0"
#define OWPAINT_COLS 32
#define OWPAINT_ROWS 16
#define OWPAINT_CELLS (OWPAINT_COLS * OWPAINT_ROWS)

static char s_canvas[OWPAINT_ROWS][OWPAINT_COLS];

/* ── Forward declarations ─────────────────────────────────────── */

static int  owpaint_run(int argc, const char *const *argv);
static void owpaint_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owpaint_usage();
        return 1;
    }
    return owpaint_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owpaint_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool owpaint_parse_i32(const char *s, int32_t *out)
{
    bool neg = false;
    int64_t v = 0;
    if (s == NULL || *s == '\0') {
        return false;
    }
    if (*s == '-') {
        neg = true;
        s++;
    }
    if (*s == '\0') {
        return false;
    }
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        v = v * 10 + (int64_t)(*s - '0');
        if (v > 100000) {
            return false;
        }
        s++;
    }
    *out = (int32_t)(neg ? -v : v);
    return true;
}

static void owpaint_plot(int x, int y, char c)
{
    if (x < 0 || y < 0 || x >= OWPAINT_COLS || y >= OWPAINT_ROWS) {
        return;
    }
    s_canvas[y][x] = c;
}

static void owpaint_clear(char c)
{
    int x;
    int y;
    for (y = 0; y < OWPAINT_ROWS; y++) {
        for (x = 0; x < OWPAINT_COLS; x++) {
            s_canvas[y][x] = c;
        }
    }
}

static void owpaint_line(int x0, int y0, int x1, int y1, char c)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err;
    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    dy = -dy;
    err = dx + dy;
    for (;;) {
        owpaint_plot(x0, y0, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        {
            int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    }
}

static void owpaint_rect(int x0, int y0, int x1, int y1, char c, bool filled)
{
    int x;
    int y;
    int lo_x = (x0 < x1) ? x0 : x1;
    int hi_x = (x0 < x1) ? x1 : x0;
    int lo_y = (y0 < y1) ? y0 : y1;
    int hi_y = (y0 < y1) ? y1 : y0;

    if (filled) {
        for (y = lo_y; y <= hi_y; y++) {
            for (x = lo_x; x <= hi_x; x++) {
                owpaint_plot(x, y, c);
            }
        }
        return;
    }
    for (x = lo_x; x <= hi_x; x++) {
        owpaint_plot(x, lo_y, c);
        owpaint_plot(x, hi_y, c);
    }
    for (y = lo_y; y <= hi_y; y++) {
        owpaint_plot(lo_x, y, c);
        owpaint_plot(hi_x, y, c);
    }
}

/* Four-way flood fill over a static LIFO of cell indices. */
static void owpaint_flood(int x, int y, char c)
{
    static int32_t stack[OWPAINT_CELLS];
    int32_t top = 0;
    char target;

    if (x < 0 || y < 0 || x >= OWPAINT_COLS || y >= OWPAINT_ROWS) {
        return;
    }
    target = s_canvas[y][x];
    if (target == c) {
        return;
    }
    stack[top++] = (int32_t)(y * OWPAINT_COLS + x);
    s_canvas[y][x] = c;
    while (top > 0) {
        int32_t idx = stack[--top];
        int cx = idx % OWPAINT_COLS;
        int cy = idx / OWPAINT_COLS;
        if (cx > 0 && s_canvas[cy][cx - 1] == target) {
            s_canvas[cy][cx - 1] = c;
            stack[top++] = (int32_t)(cy * OWPAINT_COLS + cx - 1);
        }
        if (cx + 1 < OWPAINT_COLS && s_canvas[cy][cx + 1] == target) {
            s_canvas[cy][cx + 1] = c;
            stack[top++] = (int32_t)(cy * OWPAINT_COLS + cx + 1);
        }
        if (cy > 0 && s_canvas[cy - 1][cx] == target) {
            s_canvas[cy - 1][cx] = c;
            stack[top++] = (int32_t)((cy - 1) * OWPAINT_COLS + cx);
        }
        if (cy + 1 < OWPAINT_ROWS && s_canvas[cy + 1][cx] == target) {
            s_canvas[cy + 1][cx] = c;
            stack[top++] = (int32_t)((cy + 1) * OWPAINT_COLS + cx);
        }
    }
}

static void owpaint_render(void)
{
    int x;
    int y;
    owx_puts("+");
    for (x = 0; x < OWPAINT_COLS; x++) {
        owx_putc('-');
    }
    owx_puts("+\r\n");
    for (y = 0; y < OWPAINT_ROWS; y++) {
        owx_putc('|');
        for (x = 0; x < OWPAINT_COLS; x++) {
            owx_putc(s_canvas[y][x]);
        }
        owx_puts("|\r\n");
    }
    owx_puts("+");
    for (x = 0; x < OWPAINT_COLS; x++) {
        owx_putc('-');
    }
    owx_puts("+\r\n");
}

/* ── Implementation ───────────────────────────────────────────── */

static int owpaint_run(int argc, const char *const *argv)
{
    bool verbose = false;
    int i;

    owpaint_clear('.');
    if (argc <= 1) {
        owx_puts("owpaint: 32x16 canvas, blank\r\n");
        owpaint_render();
        return 0;
    }

    i = 1;
    while (i < argc) {
        const char *cmd = argv[i];
        if (owpaint_streq(cmd, "-v") || owpaint_streq(cmd, "--verbose")) {
            verbose = true;
            i++;
            continue;
        }
        if (owpaint_streq(cmd, "-h") || owpaint_streq(cmd, "--help")) {
            owpaint_usage();
            return 0;
        }
        if (owpaint_streq(cmd, "point")) {
            int32_t x;
            int32_t y;
            if (i + 3 >= argc || !owpaint_parse_i32(argv[i + 1], &x) ||
                !owpaint_parse_i32(argv[i + 2], &y) || argv[i + 3][0] == '\0') {
                owx_puts("owpaint: point X Y C\r\n");
                return 2;
            }
            owpaint_plot(x, y, argv[i + 3][0]);
            i += 4;
        } else if (owpaint_streq(cmd, "line")) {
            int32_t x0;
            int32_t y0;
            int32_t x1;
            int32_t y1;
            if (i + 5 >= argc || !owpaint_parse_i32(argv[i + 1], &x0) ||
                !owpaint_parse_i32(argv[i + 2], &y0) ||
                !owpaint_parse_i32(argv[i + 3], &x1) ||
                !owpaint_parse_i32(argv[i + 4], &y1) || argv[i + 5][0] == '\0') {
                owx_puts("owpaint: line X0 Y0 X1 Y1 C\r\n");
                return 2;
            }
            owpaint_line(x0, y0, x1, y1, argv[i + 5][0]);
            i += 6;
        } else if (owpaint_streq(cmd, "rect") || owpaint_streq(cmd, "frect")) {
            bool filled = owpaint_streq(cmd, "frect");
            int32_t x0;
            int32_t y0;
            int32_t x1;
            int32_t y1;
            if (i + 5 >= argc || !owpaint_parse_i32(argv[i + 1], &x0) ||
                !owpaint_parse_i32(argv[i + 2], &y0) ||
                !owpaint_parse_i32(argv[i + 3], &x1) ||
                !owpaint_parse_i32(argv[i + 4], &y1) || argv[i + 5][0] == '\0') {
                owx_puts("owpaint: rect X0 Y0 X1 Y1 C\r\n");
                return 2;
            }
            owpaint_rect(x0, y0, x1, y1, argv[i + 5][0], filled);
            i += 6;
        } else if (owpaint_streq(cmd, "fill")) {
            int32_t x;
            int32_t y;
            if (i + 3 >= argc || !owpaint_parse_i32(argv[i + 1], &x) ||
                !owpaint_parse_i32(argv[i + 2], &y) || argv[i + 3][0] == '\0') {
                owx_puts("owpaint: fill X Y C\r\n");
                return 2;
            }
            owpaint_flood((int)x, (int)y, argv[i + 3][0]);
            i += 4;
        } else if (owpaint_streq(cmd, "clear")) {
            if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                owx_puts("owpaint: clear C\r\n");
                return 2;
            }
            owpaint_clear(argv[i + 1][0]);
            i += 2;
        } else if (owpaint_streq(cmd, "render")) {
            owpaint_render();
            i++;
            continue;
        } else {
            owx_puts("owpaint: unknown command '");
            owx_puts(cmd);
            owx_puts("'\r\n");
            owpaint_usage();
            return 2;
        }

        if (verbose) {
            owpaint_render();
        }
    }

    owpaint_render();
    return 0;
}

static void owpaint_usage(void)
{
    owx_puts(
        "usage: owpaint [-v] [command ...]\r\n"
        "  point X Y C                 set one cell\r\n"
        "  line X0 Y0 X1 Y1 C          Bresenham line\r\n"
        "  rect X0 Y0 X1 Y1 C          rectangle outline\r\n"
        "  frect X0 Y0 X1 Y1 C         filled rectangle\r\n"
        "  fill X Y C                  four-way flood fill\r\n"
        "  clear C                     fill the whole canvas with C\r\n"
        "  render                      draw the canvas now\r\n"
        "  -v, --verbose               render after every command\r\n"
        "  The canvas is 32x16; coordinates are clipped to it.\r\n");
}
