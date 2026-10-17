/*
 * owmediaplayer.c - OpenWindows Media Player (.owx)
 *
 * A headless model of the media player: a mutable playlist (add / remove /
 * clear), a stopped/playing/paused state machine with validated transitions,
 * and a now-playing line with a progress bar.  Playback advances only when the
 * script asks (`tick`), so results are deterministic; the end of a track
 * auto-advances the playlist and stops after the last entry.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWMEDIAPLAYER_VERSION "1.0.0"
#define OWMEDIAPLAYER_MAX_TRACKS 16u
#define OWMEDIAPLAYER_BAR_WIDTH  20u

typedef enum {
    OWMEDIAPLAYER_STOPPED = 0,
    OWMEDIAPLAYER_PLAYING,
    OWMEDIAPLAYER_PAUSED
} owmediaplayer_state_t;

typedef struct {
    const char *title;
    uint32_t    seconds;
} owmediaplayer_track_t;

/* The bundled playlist used when nothing was added. */
static const owmediaplayer_track_t owmediaplayer_preset[] = {
    { "OpenWindows Startup Chime",  12u },
    { "Kernel Waltz in D minor",   187u },
    { "OWFS Dirge",                 96u },
    { "BANcode Siren (live)",      214u }
};

static owmediaplayer_track_t s_tracks[OWMEDIAPLAYER_MAX_TRACKS];
static uint32_t s_count;
static uint32_t s_index;
static uint32_t s_pos;                    /* seconds into the current track */
static owmediaplayer_state_t s_state;

/* ── Forward declarations ─────────────────────────────────────── */

static int  owmediaplayer_run(int argc, const char *const *argv);
static void owmediaplayer_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owmediaplayer_usage();
        return 1;
    }
    return owmediaplayer_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owmediaplayer_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool owmediaplayer_parse_u32(const char *s, uint32_t *out)
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
        if (v > 100000u) {
            return false;
        }
        s++;
    }
    *out = (uint32_t)v;
    return true;
}

static void owmediaplayer_put2(uint32_t v)
{
    owx_putc((char)('0' + (int)((v / 10u) % 10u)));
    owx_putc((char)('0' + (int)(v % 10u)));
}

static void owmediaplayer_mmss(uint32_t seconds)
{
    owmediaplayer_put2(seconds / 60u);
    owx_putc(':');
    owmediaplayer_put2(seconds % 60u);
}

static const char *owmediaplayer_state_name(owmediaplayer_state_t s)
{
    switch (s) {
    case OWMEDIAPLAYER_PLAYING:
        return "PLAYING";
    case OWMEDIAPLAYER_PAUSED:
        return "PAUSED";
    case OWMEDIAPLAYER_STOPPED:
        return "STOPPED";
    default:
        return "?";
    }
}

/* Advance the playlist by `delta` entries, wrapping, then reset position. */
static void owmediaplayer_track_delta(int delta)
{
    if (s_count == 0u) {
        return;
    }
    {
        int32_t idx = (int32_t)s_index + (int32_t)delta;
        while (idx < 0) {
            idx += (int32_t)s_count;
        }
        while (idx >= (int32_t)s_count) {
            idx -= (int32_t)s_count;
        }
        s_index = (uint32_t)idx;
    }
    s_pos = 0u;
}

/* Auto-advance at end-of-track; stops after the last entry. */
static void owmediaplayer_advance_if_done(void)
{
    while (s_state == OWMEDIAPLAYER_PLAYING && s_count > 0u &&
           s_pos >= s_tracks[s_index].seconds) {
        if (s_index + 1u < s_count) {
            s_index++;
            s_pos = 0u;
        } else {
            s_state = OWMEDIAPLAYER_STOPPED;
            s_pos = s_tracks[s_index].seconds;
        }
    }
}

static void owmediaplayer_render(void)
{
    uint32_t dur;
    uint32_t pct;
    uint32_t filled;
    uint32_t k;

    if (s_count == 0u || s_index >= s_count) {
        owx_puts("owmediaplayer: [EMPTY] no tracks in playlist\r\n");
        return;
    }
    dur = s_tracks[s_index].seconds;
    pct = (dur == 0u) ? 0u : (s_pos * 100u) / dur;
    filled = (dur == 0u) ? 0u : (pct * OWMEDIAPLAYER_BAR_WIDTH) / 100u;
    if (filled > OWMEDIAPLAYER_BAR_WIDTH) {
        filled = OWMEDIAPLAYER_BAR_WIDTH;
    }

    owx_puts("owmediaplayer: [");
    owx_puts(owmediaplayer_state_name(s_state));
    owx_puts("] ");
    owx_put_u64((uint64_t)(s_index + 1u));
    owx_putc('/');
    owx_put_u64((uint64_t)s_count);
    owx_puts("  \"");
    owx_puts(s_tracks[s_index].title);
    owx_puts("\"\r\n");

    owx_puts("               ");
    owmediaplayer_mmss(s_pos);
    owx_puts(" / ");
    owmediaplayer_mmss(dur);
    owx_puts("  [");
    for (k = 0u; k < OWMEDIAPLAYER_BAR_WIDTH; k++) {
        owx_putc((k < filled) ? '#' : '.');
    }
    owx_puts("] ");
    owx_put_u64((uint64_t)pct);
    owx_puts("%\r\n");
}

static void owmediaplayer_list(void)
{
    uint32_t i;
    owx_puts("owmediaplayer: playlist (");
    owx_put_u64((uint64_t)s_count);
    owx_puts(" tracks):\r\n");
    for (i = 0u; i < s_count; i++) {
        owx_puts("  ");
        if (i == s_index) {
            owx_puts("> ");
        } else {
            owx_puts("  ");
        }
        owx_put_u64((uint64_t)(i + 1u));
        owx_puts(". ");
        owx_puts(s_tracks[i].title);
        owx_puts(" (");
        owmediaplayer_mmss(s_tracks[i].seconds);
        owx_puts(")\r\n");
    }
}

static void owmediaplayer_advance(uint32_t seconds)
{
    while (seconds > 0u) {
        uint32_t step;
        if (s_count == 0u || s_index >= s_count) {
            return;
        }
        step = s_tracks[s_index].seconds - s_pos;
        if (s_state != OWMEDIAPLAYER_PLAYING) {
            return;
        }
        if (step == 0u) {
            owmediaplayer_advance_if_done();
            if (s_state != OWMEDIAPLAYER_PLAYING) {
                return;
            }
            step = 1u;
        }
        if (step > seconds) {
            s_pos += seconds;
            seconds = 0u;
        } else {
            s_pos += step;
            seconds -= step;
        }
        owmediaplayer_advance_if_done();
    }
}

/* ── Implementation ───────────────────────────────────────────── */

static int owmediaplayer_run(int argc, const char *const *argv)
{
    int i;
    int bad = 0;

    s_count = 0u;
    s_index = 0u;
    s_pos = 0u;
    s_state = OWMEDIAPLAYER_STOPPED;

    for (i = 1; i < argc; i++) {
        const char *cmd = argv[i];
        if (cmd == NULL) {
            continue;
        }
        if (owmediaplayer_streq(cmd, "add")) {
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                owx_puts("owmediaplayer: add requires a title\r\n");
                bad = 1;
                continue;
            }
            if (s_count >= OWMEDIAPLAYER_MAX_TRACKS) {
                owx_puts("owmediaplayer: playlist is full\r\n");
                bad = 1;
                continue;
            }
            {
                const char *title = argv[++i];
                uint32_t secs = 120u;
                if (i + 1 < argc && argv[i + 1] != NULL) {
                    uint32_t v;
                    if (owmediaplayer_parse_u32(argv[i + 1], &v)) {
                        secs = v;
                        i++;
                    }
                }
                s_tracks[s_count].title = title;
                s_tracks[s_count].seconds = secs;
                s_count++;
                owx_puts("owmediaplayer: added \"");
                owx_puts(title);
                owx_puts("\"\r\n");
            }
        } else if (owmediaplayer_streq(cmd, "remove")) {
            uint32_t n = 0u;
            uint32_t k;
            if (i + 1 >= argc || !owmediaplayer_parse_u32(argv[i + 1], &n) ||
                n == 0u || n > s_count) {
                owx_puts("owmediaplayer: remove needs a valid 1-based index\r\n");
                bad = 1;
                continue;
            }
            i++;
            for (k = n - 1u; k + 1u < s_count; k++) {
                s_tracks[k] = s_tracks[k + 1u];
            }
            s_count--;
            if (s_index >= s_count && s_count > 0u) {
                s_index = s_count - 1u;
            }
            owx_puts("owmediaplayer: removed track ");
            owx_put_u64((uint64_t)n);
            owx_puts("\r\n");
        } else if (owmediaplayer_streq(cmd, "next")) {
            owmediaplayer_track_delta(1);
        } else if (owmediaplayer_streq(cmd, "prev")) {
            owmediaplayer_track_delta(-1);
        } else if (owmediaplayer_streq(cmd, "play")) {
            if (s_count == 0u) {
                owx_puts("owmediaplayer: playlist is empty\r\n");
            } else if (s_state == OWMEDIAPLAYER_STOPPED) {
                if (s_pos >= s_tracks[s_index].seconds) {
                    s_pos = 0u;
                }
                s_state = OWMEDIAPLAYER_PLAYING;
            } else if (s_state == OWMEDIAPLAYER_PAUSED) {
                s_state = OWMEDIAPLAYER_PLAYING;
            }
        } else if (owmediaplayer_streq(cmd, "pause")) {
            if (s_state == OWMEDIAPLAYER_PLAYING) {
                s_state = OWMEDIAPLAYER_PAUSED;
            }
        } else if (owmediaplayer_streq(cmd, "stop")) {
            s_state = OWMEDIAPLAYER_STOPPED;
            s_pos = 0u;
        } else if (owmediaplayer_streq(cmd, "seek")) {
            uint32_t v = 0u;
            if (i + 1 >= argc || !owmediaplayer_parse_u32(argv[i + 1], &v)) {
                owx_puts("owmediaplayer: seek needs a second offset\r\n");
                bad = 1;
                continue;
            }
            i++;
            if (s_count > 0u) {
                s_pos = (v > s_tracks[s_index].seconds) ? s_tracks[s_index].seconds : v;
            }
        } else if (owmediaplayer_streq(cmd, "tick")) {
            uint32_t v = 0u;
            if (i + 1 >= argc || !owmediaplayer_parse_u32(argv[i + 1], &v) ||
                v == 0u) {
                owx_puts("owmediaplayer: tick needs a positive second count\r\n");
                bad = 1;
                continue;
            }
            i++;
            owmediaplayer_advance(v);
        } else if (owmediaplayer_streq(cmd, "clear")) {
            s_count = 0u;
            s_index = 0u;
            s_pos = 0u;
            s_state = OWMEDIAPLAYER_STOPPED;
        } else if (owmediaplayer_streq(cmd, "list")) {
            owmediaplayer_list();
            continue;
        } else {
            owx_puts("owmediaplayer: unknown command '");
            owx_puts(cmd);
            owx_puts("'\r\n");
            bad = 1;
            continue;
        }

        owmediaplayer_advance_if_done();
        owmediaplayer_render();
    }

    if (argc <= 1) {
        uint32_t k;
        for (k = 0u; k < (uint32_t)(sizeof(owmediaplayer_preset) /
                                     sizeof(owmediaplayer_preset[0])); k++) {
            s_tracks[k].title = owmediaplayer_preset[k].title;
            s_tracks[k].seconds = owmediaplayer_preset[k].seconds;
            s_count++;
        }
        owmediaplayer_list();
        owmediaplayer_render();
    }

    return bad;
}

static void owmediaplayer_usage(void)
{
    owx_puts(
        "usage: owmediaplayer [command ...]\r\n"
        "  add <title> [seconds]   append a track\r\n"
        "  remove <n>              delete track n (1-based)\r\n"
        "  next | prev             move the current track\r\n"
        "  play | pause | stop     drive the player state machine\r\n"
        "  seek <seconds>          set position inside the current track\r\n"
        "  tick <seconds>          advance playback; end-of-track auto-skips\r\n"
        "  list                    print the playlist\r\n"
        "  clear                   empty the playlist\r\n"
        "  With no arguments the bundled playlist is listed and rendered.\r\n");
}
