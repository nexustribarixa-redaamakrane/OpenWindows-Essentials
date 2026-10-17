/*
 * owstartmenu.c - OpenWindows Start Menu Application (.owx)
 *
 * A headless model of the shell's start menu.  It renders a list of launchable
 * entries, accepts a selection by number or by name, and launches the chosen
 * entry through the Ring 3 spawn gateway.  `-l` lists the entries without
 * launching anything.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWSTARTMENU_VERSION "1.0.0"

typedef struct {
    const char *name;
    const char *image;
    const char *hint;
} owstartmenu_entry_t;

static const owstartmenu_entry_t s_entries[] = {
    { "Documents",     "owfm.owx",          "browse the OWFS volume" },
    { "Terminal",      "owterm.owx",        "text console session" },
    { "Notepad",       "ownotepad.owx",     "edit a text buffer" },
    { "Paint",         "owpaint.owx",       "draw on a character canvas" },
    { "Calculator",    "owcalcgui.owx",     "evaluate expressions" },
    { "Media Player",  "owmediaplayer.owx", "play the playlist" },
    { "Settings",      "owsettings.owx",    "system preferences" },
    { "Screenshot",    "owscreenshot.owx",  "capture the framebuffer" },
    { "Help",          "owabout.owx",       "about OpenWindows" }
};
#define OWSTARTMENU_COUNT (sizeof(s_entries) / sizeof(s_entries[0]))

/* ── Forward declarations ─────────────────────────────────────── */

static int  owstartmenu_run(int argc, const char *const *argv);
static void owstartmenu_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owstartmenu_usage();
        return 1;
    }
    return owstartmenu_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static char owstartmenu_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static bool owstartmenu_streq_ci(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0' &&
           owstartmenu_lower(*a) == owstartmenu_lower(*b)) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool owstartmenu_parse_u32(const char *s, uint32_t *out)
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

static void owstartmenu_pad(const char *s, uint32_t width)
{
    uint32_t n = 0u;
    while (*s != '\0' && n < width) {
        owx_putc(*s++);
        n++;
    }
    while (n < width) {
        owx_putc(' ');
        n++;
    }
}

static void owstartmenu_render(void)
{
    uint32_t i;
    owx_puts("+-- OpenWindows Start Menu ------------------+\r\n");
    for (i = 0u; i < OWSTARTMENU_COUNT; i++) {
        owx_puts("| ");
        owx_put_u64((uint64_t)(i + 1u));
        owx_puts(". ");
        owstartmenu_pad(s_entries[i].name, 16u);
        owstartmenu_pad(s_entries[i].hint, 18u);
        owx_puts("|\r\n");
    }
    owx_puts("+--------------------------------------------+\r\n");
}

static void owstartmenu_list(void)
{
    uint32_t i;
    owx_puts("owstartmenu: entries (");
    owx_put_u64((uint64_t)OWSTARTMENU_COUNT);
    owx_puts("):\r\n");
    for (i = 0u; i < OWSTARTMENU_COUNT; i++) {
        owx_puts("  ");
        owx_put_u64((uint64_t)(i + 1u));
        owx_puts(". ");
        owx_puts(s_entries[i].name);
        owx_puts(" -> ");
        owx_puts(s_entries[i].image);
        owx_puts("\r\n");
    }
}

static void owstartmenu_launch(uint32_t index, bool no_spawn)
{
    if (index < 1u || index > OWSTARTMENU_COUNT) {
        owx_puts("owstartmenu: no entry ");
        owx_put_u64((uint64_t)index);
        owx_puts("\r\n");
        return;
    }
    {
        const owstartmenu_entry_t *e = &s_entries[index - 1u];
        if (no_spawn) {
            owx_puts("owstartmenu: (dry run) would launch ");
            owx_puts(e->name);
            owx_puts(" from ");
            owx_puts(e->image);
            owx_puts("\r\n");
            return;
        }
        owx_puts("owstartmenu: launching ");
        owx_puts(e->name);
        owx_puts(" (");
        owx_puts(e->image);
        owx_puts(")");
        {
            int64_t pid = owx_spawn(e->image);
            if (pid >= 0) {
                owx_puts(" -> pid ");
                owx_put_i64(pid);
                owx_puts("\r\n");
            } else {
                owx_puts(" -> spawn failed (");
                owx_put_i64(pid);
                owx_puts(")\r\n");
            }
        }
    }
}

static bool owstartmenu_by_name(const char *name, uint32_t *index)
{
    uint32_t i;
    for (i = 0u; i < OWSTARTMENU_COUNT; i++) {
        if (owstartmenu_streq_ci(s_entries[i].name, name)) {
            *index = i + 1u;
            return true;
        }
    }
    return false;
}

/* ── Implementation ───────────────────────────────────────────── */

static int owstartmenu_run(int argc, const char *const *argv)
{
    bool no_spawn = false;
    bool listed = false;
    bool launched = false;
    int i;

    if (argc <= 1) {
        owstartmenu_render();
        owx_puts("owstartmenu: pick an entry by number or name, or use -l to list\r\n");
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        uint32_t index = 0u;
        if (arg == NULL) {
            continue;
        }
        if (owstartmenu_streq_ci(arg, "-l") || owstartmenu_streq_ci(arg, "--list")) {
            owstartmenu_list();
            listed = true;
            continue;
        }
        if (owstartmenu_streq_ci(arg, "-h") || owstartmenu_streq_ci(arg, "--help")) {
            owstartmenu_usage();
            return 0;
        }
        if (owstartmenu_streq_ci(arg, "--no-spawn")) {
            no_spawn = true;
            continue;
        }
        if (owstartmenu_parse_u32(arg, &index)) {
            owstartmenu_launch(index, no_spawn);
            launched = true;
            continue;
        }
        if (owstartmenu_by_name(arg, &index)) {
            owstartmenu_launch(index, no_spawn);
            launched = true;
            continue;
        }
        owx_puts("owstartmenu: no such entry '");
        owx_puts(arg);
        owx_puts("'\r\n");
        owstartmenu_usage();
        return 2;
    }

    if (!listed && !launched) {
        owstartmenu_render();
    }
    return 0;
}

static void owstartmenu_usage(void)
{
    owx_puts(
        "usage: owstartmenu [-l] [--no-spawn] [entry ...] [-h]\r\n"
        "  -l, --list        list the entries and their images\r\n"
        "  entry             an entry number (1..N) or an entry name\r\n"
        "  --no-spawn        resolve a selection without launching it\r\n"
        "  -h, --help        show this help\r\n"
        "  With no arguments the menu is drawn.\r\n");
}
