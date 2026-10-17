/*
 * owclock.c - OpenWindows Clock Widget (.owx)
 *
 * Formats a modelled wall clock and draws it as a text clock face: an ASCII
 * analog dial (perimeter, hour ticks, and hour/minute/second hands placed with
 * a fixed-point sine table) plus a zero-padded digital readout and weekday.
 * Time is derived arithmetically from an epoch (no time syscall exists), and
 * `-n` frames animate the dial one modelled second per frame.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWCLOCK_VERSION "1.0.0"

/* Dial geometry.  Odd dimensions keep the centre on an exact cell. */
#define OWCLOCK_COLS 31u
#define OWCLOCK_ROWS 15u
#define OWCLOCK_CX   15
#define OWCLOCK_CY   7

/* Fixed-point sine table: sin(deg) * 1000 for deg in 0..90. */
static const int32_t owclock_sin_q[91] = {
       0,   17,   35,   52,   70,   87,  105,  122,  139,  156,
     174,  191,  208,  225,  242,  259,  276,  292,  309,  326,
     342,  358,  375,  391,  407,  423,  438,  454,  469,  485,
     500,  515,  530,  545,  559,  574,  588,  602,  616,  629,
     643,  656,  669,  682,  695,  707,  719,  731,  743,  755,
     766,  777,  788,  799,  809,  819,  829,  839,  848,  857,
     866,  875,  883,  891,  899,  906,  914,  921,  927,  934,
     940,  946,  951,  956,  961,  966,  970,  974,  978,  982,
     985,  988,  990,  992,  995,  996,  998,  999,  999, 1000,
    1000
};

static const char *const owclock_days[7] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static char s_grid[OWCLOCK_ROWS][OWCLOCK_COLS + 1u];

/* ── Forward declarations ─────────────────────────────────────── */

static int  owclock_run(int argc, const char *const *argv);
static void owclock_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owclock_usage();
        return 1;
    }
    return owclock_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owclock_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

/* Sine of a normalised degree angle, scaled by 1000. */
static int32_t owclock_sin_deg(int deg)
{
    int q = deg % 360;
    if (q < 0) {
        q += 360;
    }
    if (q <= 90) {
        return owclock_sin_q[q];
    }
    if (q <= 180) {
        return owclock_sin_q[180 - q];
    }
    if (q <= 270) {
        return -owclock_sin_q[q - 180];
    }
    return -owclock_sin_q[360 - q];
}

static int32_t owclock_cos_deg(int deg)
{
    return owclock_sin_deg(deg + 90);
}

static void owclock_plot(int x, int y, char c)
{
    if (x < 0 || y < 0 || x >= (int)OWCLOCK_COLS || y >= (int)OWCLOCK_ROWS) {
        return;
    }
    s_grid[y][x] = c;
}

/* Integer Bresenham line, used for the hands and the hour ticks. */
static void owclock_line(int x0, int y0, int x1, int y1, char c)
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
        int e2;
        owclock_plot(x0, y0, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        e2 = 2 * err;
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

/* Draw a radial spoke at `deg` from inner to outer radii. */
static void owclock_spoke(int deg, int in_x, int in_y, int out_x, int out_y,
                          char c)
{
    int32_t sn = owclock_sin_deg(deg);
    int32_t cs = owclock_cos_deg(deg);
    int x0 = (int)OWCLOCK_CX + (int)((in_x * cs) / 1000);
    int y0 = (int)OWCLOCK_CY - (int)((in_y * sn) / 1000);
    int x1 = (int)OWCLOCK_CX + (int)((out_x * cs) / 1000);
    int y1 = (int)OWCLOCK_CY - (int)((out_y * sn) / 1000);
    owclock_line(x0, y0, x1, y1, c);
}

static void owclock_clear_grid(void)
{
    uint32_t r;
    uint32_t cc;
    for (r = 0u; r < OWCLOCK_ROWS; r++) {
        for (cc = 0u; cc < OWCLOCK_COLS; cc++) {
            s_grid[r][cc] = ' ';
        }
        s_grid[r][OWCLOCK_COLS] = '\0';
    }
}

static void owclock_draw_face(uint32_t hour, uint32_t minute, uint32_t second)
{
    int deg;
    int h;

    owclock_clear_grid();

    /* Dial perimeter. */
    for (deg = 0; deg < 360; deg++) {
        int32_t sn = owclock_sin_deg(deg);
        int32_t cs = owclock_cos_deg(deg);
        int x = (int)OWCLOCK_CX + (int)((15 * cs) / 1000);
        int y = (int)OWCLOCK_CY - (int)((7 * sn) / 1000);
        owclock_plot(x, y, '.');
    }

    /* Twelve hour ticks. */
    for (h = 0; h < 12; h++) {
        owclock_spoke(h * 30, 12, 6, 15, 7, '*');
    }

    /* Hands, longest drawn first so the shorter ones sit on top at the hub. */
    owclock_spoke((int)((hour % 12u) * 30u + minute / 2u), 0, 0, 8, 4, 'H');
    owclock_spoke((int)(minute * 6u + second / 10u), 0, 0, 11, 5, 'M');
    owclock_spoke((int)(second * 6u), 0, 0, 13, 6, 'S');
    owclock_plot((int)OWCLOCK_CX, (int)OWCLOCK_CY, '@');
}

static void owclock_put2(uint32_t v)
{
    owx_putc((char)('0' + (int)((v / 10u) % 10u)));
    owx_putc((char)('0' + (int)(v % 10u)));
}

static void owclock_render(uint32_t epoch)
{
    uint32_t daysec = epoch % 86400u;
    uint32_t hour = daysec / 3600u;
    uint32_t minute = (daysec % 3600u) / 60u;
    uint32_t second = daysec % 60u;
    uint32_t dow = (uint32_t)(((epoch / 86400u) + 4u) % 7u);
    uint32_t r;

    owx_puts("\x1B[H\x1B[2J");

    owclock_draw_face(hour, minute, second);
    for (r = 0u; r < OWCLOCK_ROWS; r++) {
        owx_puts(s_grid[r]);
        owx_puts("\r\n");
    }

    owx_puts("owclock: ");
    owclock_put2(hour);
    owx_putc(':');
    owclock_put2(minute);
    owx_putc(':');
    owclock_put2(second);
    owx_puts(" UTC ");
    owx_puts(owclock_days[dow]);
    owx_puts("  epoch=");
    owx_put_u64((uint64_t)epoch);
    owx_puts("s\r\n");
}

/* Parse "H[:MM[:SS]]" into seconds within a day. */
static bool owclock_parse_clock(const char *s, uint32_t *out)
{
    uint32_t field[3] = { 0u, 0u, 0u };
    int idx = 0;
    bool any = false;

    if (s == NULL) {
        return false;
    }
    while (*s != '\0') {
        if (*s == ':') {
            if (!any || idx >= 2) {
                return false;
            }
            idx++;
            any = false;
        } else if (*s >= '0' && *s <= '9') {
            field[idx] = field[idx] * 10u + (uint32_t)(*s - '0');
            if (field[idx] > 100000u) {
                return false;
            }
            any = true;
        } else {
            return false;
        }
        s++;
    }
    if (!any) {
        return false;
    }
    if (field[0] > 23u || field[1] > 59u || field[2] > 59u) {
        return false;
    }
    *out = field[0] * 3600u + field[1] * 60u + field[2];
    return true;
}

/* Parse a non-negative epoch in seconds. */
static bool owclock_parse_epoch(const char *s, uint32_t *out)
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
        if (v > 4294967295u) {
            return false;
        }
        s++;
    }
    *out = (uint32_t)v;
    return true;
}

static bool owclock_contains(const char *s, char c)
{
    while (*s != '\0') {
        if (*s == c) {
            return true;
        }
        s++;
    }
    return false;
}

/* ── Implementation ───────────────────────────────────────────── */

static int owclock_run(int argc, const char *const *argv)
{
    uint32_t epoch = 0u;
    uint32_t frames = 1u;
    int64_t sleep_ticks = 100;    /* 100 Hz scheduler: one modelled second */
    bool analog = true;
    uint32_t f;
    int i;

    for (i = 1; i < argc; i++) {
        const char *opt = argv[i];
        if (opt == NULL) {
            continue;
        }
        if (owclock_streq(opt, "-t") || owclock_streq(opt, "--time")) {
            uint32_t t = 0u;
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                owx_puts("owclock: -t requires an epoch or HH:MM:SS\r\n");
                owclock_usage();
                return 2;
            }
            if (owclock_contains(argv[i + 1], ':')) {
                if (!owclock_parse_clock(argv[i + 1], &t)) {
                    owx_puts("owclock: bad time, expected HH:MM:SS\r\n");
                    return 2;
                }
            } else if (!owclock_parse_epoch(argv[i + 1], &t)) {
                owx_puts("owclock: bad epoch, expected seconds\r\n");
                return 2;
            }
            epoch = t;
            i++;
        } else if (owclock_streq(opt, "-n") || owclock_streq(opt, "--frames")) {
            uint32_t n = 0u;
            if (!owclock_parse_epoch((i + 1 < argc) ? argv[i + 1] : NULL, &n) ||
                n == 0u || n > 100000u) {
                owx_puts("owclock: -n requires a frame count in 1..100000\r\n");
                owclock_usage();
                return 2;
            }
            frames = n;
            i++;
        } else if (owclock_streq(opt, "-d") || owclock_streq(opt, "--tick")) {
            uint32_t t = 0u;
            if (!owclock_parse_epoch((i + 1 < argc) ? argv[i + 1] : NULL, &t) ||
                t > 1000u) {
                owx_puts("owclock: -d requires a tick delay in 0..1000\r\n");
                owclock_usage();
                return 2;
            }
            sleep_ticks = (int64_t)t;
            i++;
        } else if (owclock_streq(opt, "-D") || owclock_streq(opt, "--digital")) {
            analog = false;
        } else if (owclock_streq(opt, "-h") || owclock_streq(opt, "--help")) {
            owclock_usage();
            return 0;
        } else {
            owx_puts("owclock: unknown option '");
            owx_puts(opt);
            owx_puts("'\r\n");
            owclock_usage();
            return 2;
        }
    }

    for (f = 0u; f < frames; f++) {
        if (analog) {
            owclock_render(epoch);
        } else {
            owx_puts("\x1B[H\x1B[2J");
            owx_puts("owclock: digital display (UTC, modelled)\r\n");
            {
                uint32_t daysec = epoch % 86400u;
                uint32_t dow = (uint32_t)(((epoch / 86400u) + 4u) % 7u);
                owx_puts("owclock: ");
                owclock_put2(daysec / 3600u);
                owx_putc(':');
                owclock_put2((daysec % 3600u) / 60u);
                owx_putc(':');
                owclock_put2(daysec % 60u);
                owx_puts(" UTC ");
                owx_puts(owclock_days[dow]);
                owx_puts("\r\n");
            }
        }
        epoch++;
        if (f + 1u < frames && sleep_ticks > 0) {
            owx_sleep(sleep_ticks);
        }
    }

    return 0;
}

static void owclock_usage(void)
{
    owx_puts(
        "usage: owclock [-t epoch|HH:MM:SS] [-n frames] [-d ticks] [-D] [-h]\r\n"
        "  -t, --time T     starting time as epoch seconds or HH:MM:SS (default 0)\r\n"
        "  -n, --frames N   draw N frames, one modelled second apart (default 1)\r\n"
        "  -d, --tick N     scheduler ticks to sleep between frames (default 100)\r\n"
        "  -D, --digital    print only the digital readout, no analog dial\r\n"
        "  -h, --help       show this help\r\n");
}
