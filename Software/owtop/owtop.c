/*
 * owtop.c - OpenWindows Executive Performance & Task Monitor (.owx)
 *
 * Monitors active processes, executive threads, CPU utilization, and
 * memory commit charges across system and interactive sessions.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owtop snapshot                  Render a single executive performance snapshot
 *   owtop monitor [frames] [delay]  Stream consecutive performance frames
 *   owtop query <pid>               Display details for a specific process ID
 *   owtop help                      Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWTOP_VERSION        "1.2.0"
#define OWTOP_MAX_PROCS      16u
#define OWTOP_DEFAULT_SLEEP  100u       /* 100 Hz scheduler ticks */
#define OWTOP_BASE_UPTIME    187200u     /* modeled boot uptime, seconds */

/* ── Process Model ────────────────────────────────────────────── */

typedef struct {
    uint32_t    pid;
    uint32_t    session;
    const char *state;
    const char *image;
    uint32_t    cpu_tenths;
    uint32_t    ws_kb;
    uint32_t    threads;
} owtop_proc_t;

static const owtop_proc_t owtop_model[] = {
    { 0u,  0u, "IDLE", "System Idle Process",    0u,     0u, 4u },
    { 1u,  0u, "RUN",  "owinit.owx",             4u,  1024u, 2u },
    { 4u,  0u, "WAIT", "owserviced.owx",        12u,  2048u, 6u },
    { 7u,  0u, "WAIT", "owdllhost.owx",          6u,  1536u, 3u },
    { 12u, 1u, "RUN",  "owsh.owx",              28u,  4096u, 2u },
    { 21u, 1u, "RUN",  "owtop.owx",            115u,  3072u, 1u },
    { 33u, 1u, "WAIT", "owedit.owx",            18u,  6144u, 2u }
};

/* ── String and Number Helpers ────────────────────────────────── */

static uint32_t owtop_strlen(const char *s)
{
    uint32_t n = 0u;
    if (!s) return 0u;
    while (s[n] != '\0') n++;
    return n;
}

static bool owtop_streq(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return *a == *b;
}

static void owtop_pad(uint32_t count)
{
    while (count-- > 0u) {
        owx_putc(' ');
    }
}

static void owtop_put_right(const char *s, uint32_t width)
{
    uint32_t len = owtop_strlen(s);
    if (len < width) {
        owtop_pad(width - len);
    }
    owx_puts(s);
}

static void owtop_put_left(const char *s, uint32_t width)
{
    uint32_t len = owtop_strlen(s);
    owx_puts(s);
    if (len < width) {
        owtop_pad(width - len);
    }
}

static void owtop_fmt_u64(uint64_t val, char *buf, uint32_t cap)
{
    char tmp[21];
    uint32_t tn = 0u;
    uint32_t on = 0u;
    if (cap == 0u) return;
    if (val == 0u) {
        buf[0] = '0';
        if (cap > 1u) buf[1] = '\0';
        return;
    }
    while (val > 0u && tn < 20u) {
        tmp[tn++] = (char)('0' + (val % 10u));
        val /= 10u;
    }
    while (tn > 0u && on + 1u < cap) {
        buf[on++] = tmp[--tn];
    }
    buf[on] = '\0';
}

static void owtop_put_right_u64(uint64_t val, uint32_t width)
{
    char buf[24];
    owtop_fmt_u64(val, buf, sizeof(buf));
    owtop_put_right(buf, width);
}

static void owtop_fmt_cpu(uint32_t tenths, char *buf, uint32_t cap)
{
    char whole[16];
    char frac[4];
    uint32_t w = tenths / 10u;
    uint32_t f = tenths % 10u;
    uint32_t wn;
    uint32_t i;
    uint32_t o = 0u;

    owtop_fmt_u64((uint64_t)w, whole, sizeof(whole));
    owtop_fmt_u64((uint64_t)f, frac, sizeof(frac));
    wn = owtop_strlen(whole);

    for (i = 0u; i < wn && o + 1u < cap; i++) {
        buf[o++] = whole[i];
    }
    if (o + 1u < cap) buf[o++] = '.';
    if (o + 1u < cap) buf[o++] = frac[0];
    if (o + 1u < cap) buf[o++] = '%';
    buf[o] = '\0';
}

static uint32_t owtop_parse_u32(const char *s)
{
    uint32_t v = 0u;
    if (!s) return 0u;
    while (*s >= '0' && *s <= '9') {
        v = v * 10u + (uint32_t)(*s - '0');
        s++;
    }
    return v;
}

static void owtop_print_uptime(uint64_t sec)
{
    uint64_t days = sec / 86400u;
    uint64_t rem = sec % 86400u;
    uint64_t hrs = rem / 3600u;
    rem %= 3600u;
    uint64_t mins = rem / 60u;

    if (days > 0u) {
        owx_put_u64(days);
        owx_puts("d ");
    }
    if (hrs < 10u) owx_putc('0');
    owx_put_u64(hrs);
    owx_putc(':');
    if (mins < 10u) owx_putc('0');
    owx_put_u64(mins);
}

static void owtop_render_frame(uint32_t frame, uint32_t total_frames, uint64_t delay)
{
    const uint32_t nprocs = (uint32_t)(sizeof(owtop_model) / sizeof(owtop_model[0]));
    uint64_t uptime = (uint64_t)OWTOP_BASE_UPTIME + (uint64_t)frame * (delay / 100u);
    uint32_t total_threads = 0u;
    uint32_t total_ws = 0u;

    for (uint32_t i = 0u; i < nprocs; i++) {
        total_threads += owtop_model[i].threads;
        total_ws += owtop_model[i].ws_kb;
    }

    owx_puts("OpenWindows Executive Task Monitor v" OWTOP_VERSION "   System Uptime: ");
    owtop_print_uptime(uptime);
    owx_puts("   Frame [");
    owx_put_u64((uint64_t)(frame + 1u));
    owx_putc('/');
    owx_put_u64((uint64_t)total_frames);
    owx_puts("]\r\n");

    owx_puts("Processes: ");
    owx_put_u64((uint64_t)nprocs);
    owx_puts(" | Executive Threads: ");
    owx_put_u64((uint64_t)total_threads);
    owx_puts(" | Commit Charge: ");
    owx_put_u64((uint64_t)total_ws);
    owx_puts(" KB / 65536 KB | Kernel Pool: NonPaged\r\n");

    owx_puts("  PID  SESSION  STATE     CPU%    WS (KB)  THREADS  IMAGE\r\n");
    owx_puts("-----  -------  ------  ------  ---------  -------  --------------------\r\n");

    for (uint32_t i = 0u; i < nprocs; i++) {
        const owtop_proc_t *p = &owtop_model[i];
        char cpubuf[16];
        char sesbuf[12];
        owtop_fmt_cpu(p->cpu_tenths, cpubuf, sizeof(cpubuf));

        sesbuf[0] = 'S'; sesbuf[1] = 'E'; sesbuf[2] = 'S'; sesbuf[3] = ' ';
        sesbuf[4] = (char)('0' + p->session);
        sesbuf[5] = '\0';

        owtop_put_right_u64((uint64_t)p->pid, 5u);
        owx_puts("  ");
        owtop_put_left(sesbuf, 7u);
        owx_puts("  ");
        owtop_put_left(p->state, 6u);
        owx_puts("  ");
        owtop_put_right(cpubuf, 6u);
        owx_puts("  ");
        owtop_put_right_u64((uint64_t)p->ws_kb, 9u);
        owx_puts("  ");
        owtop_put_right_u64((uint64_t)p->threads, 7u);
        owx_puts("  ");
        owx_puts(p->image);
        owx_puts("\r\n");
    }
}

static void owtop_usage(void)
{
    owx_puts("owtop: OpenWindows Executive Task Monitor v" OWTOP_VERSION "\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owtop snapshot                  render single snapshot\r\n");
    owx_puts("  owtop monitor [frames] [delay]  stream consecutive frames (100 Hz ticks)\r\n");
    owx_puts("  owtop query <pid>               display details for process ID\r\n");
    owx_puts("  owtop help                      display this information\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb = "snapshot";
    uint32_t frames = 1u;
    uint64_t delay = OWTOP_DEFAULT_SLEEP;

    if (argc >= 2) {
        if (owtop_streq(argv[1], "help") || owtop_streq(argv[1], "-h") || owtop_streq(argv[1], "--help")) {
            owtop_usage();
            return 0;
        }
        if (owtop_streq(argv[1], "snapshot")) {
            verb = "snapshot";
        } else if (owtop_streq(argv[1], "monitor")) {
            verb = "monitor";
            if (argc >= 3) {
                frames = owtop_parse_u32(argv[2]);
                if (frames == 0u) frames = 5u;
            } else {
                frames = 5u;
            }
            if (argc >= 4) {
                delay = (uint64_t)owtop_parse_u32(argv[3]);
                if (delay == 0u) delay = OWTOP_DEFAULT_SLEEP;
            }
        } else if (owtop_streq(argv[1], "query")) {
            verb = "query";
            if (argc < 3) {
                owx_puts("owtop: query requires a PID argument\r\n");
                return 1;
            }
            uint32_t qpid = owtop_parse_u32(argv[2]);
            const uint32_t nprocs = (uint32_t)(sizeof(owtop_model) / sizeof(owtop_model[0]));
            for (uint32_t i = 0u; i < nprocs; i++) {
                if (owtop_model[i].pid == qpid) {
                    owx_puts("PID:          "); owx_put_u64((uint64_t)owtop_model[i].pid); owx_puts("\r\n");
                    owx_puts("Image:        "); owx_puts(owtop_model[i].image); owx_puts("\r\n");
                    owx_puts("Session:      SES "); owx_put_u64((uint64_t)owtop_model[i].session); owx_puts("\r\n");
                    owx_puts("State:        "); owx_puts(owtop_model[i].state); owx_puts("\r\n");
                    owx_puts("Working Set:  "); owx_put_u64((uint64_t)owtop_model[i].ws_kb); owx_puts(" KB\r\n");
                    owx_puts("Threads:      "); owx_put_u64((uint64_t)owtop_model[i].threads); owx_puts("\r\n");
                    return 0;
                }
            }
            owx_puts("owtop: PID not found\r\n");
            return 1;
        } else if (argv[1][0] == '-' && argv[1][1] == 'n' && argc >= 3) {
            verb = "monitor";
            frames = owtop_parse_u32(argv[2]);
            if (frames == 0u) frames = 1u;
        }
    }

    if (owtop_streq(verb, "snapshot")) {
        owtop_render_frame(0u, 1u, delay);
        return 0;
    }

    for (uint32_t f = 0u; f < frames; f++) {
        owtop_render_frame(f, frames, delay);
        if (f + 1u < frames) {
            owx_sleep((int64_t)delay);
        }
    }
    return 0;
}
