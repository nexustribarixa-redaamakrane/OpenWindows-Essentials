/*
 * startwm.c - OpenWindows Window Manager Launcher & Desktop Orchestrator (.owx)
 *
 * Provides the user-facing command to initialize display hardware, load Cairo64/
 * userinterface64 graphics pipelines, launch owwm compositor, and spawn desktop
 * accessories (taskbar, start menu, system monitor, and terminal).
 *
 * Equivalent to 'startx' on X11, 'startplasma' on Wayland, or 'win' on MS-DOS.
 *
 * The Ring 3 gateway (Software/owx_gate.h) has no display syscall yet, so the
 * session is assembled in the window-management library and reported over the
 * serial console; that report is what an operator sees from the boot log.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../DLL/owwm64/owwm64.h"
#include "../owx_gate.h"

#define STARTWM_VERSION "1.0.0"
#define DEFAULT_SCREEN_WIDTH  1920
#define DEFAULT_SCREEN_HEIGHT 1080

static owwm_desktop_t g_desktop_session;
static bool           g_session_running = false;

/* ── Helper Functions ─────────────────────────────────────────── */

static bool str_equals(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

/* ── Display & Hardware Preparation ───────────────────────────── */

static bool startwm_prepare_display(int32_t width, int32_t height)
{
    (void)width;
    (void)height;
    /* In bare-metal boot: vbedrv / fbdev initializes video mode 32-bpp */
    return true;
}

/* ── Desktop Session Execution ────────────────────────────────── */

static int startwm_launch_session(int32_t width, int32_t height, bool safe_mode)
{
    uint32_t i;

    if (g_session_running) {
        /* Already running */
        owx_puts("startwm: a session is already running\r\n");
        return 1;
    }

    if (!startwm_prepare_display(width, height)) {
        owx_puts("startwm: display preparation failed\r\n");
        return 2;
    }

    owx_puts("startwm: starting a ");
    owx_put_i64(width);
    owx_puts("x");
    owx_put_i64(height);
    owx_puts(safe_mode ? " safe-mode session\r\n" : " desktop session\r\n");

    /* 1. Initialize Window Manager State */
    owwm64_init(&g_desktop_session);
    g_desktop_session.cursor_x = width / 2;
    g_desktop_session.cursor_y = height / 2;
    g_desktop_session.cursor_visible = true;

    /* 2. Spawn System Panel & Desktop Windows */
    if (safe_mode) {
        /* Safe-mode GUI: only single emergency terminal window */
        owwm64_create_window(&g_desktop_session, 1u, "OpenWindows Recovery Terminal (Safe Mode)",
                             80, 80, 720, 480);
    } else {
        /* Standard desktop session: Taskbar, Shell, System Status */
        owwm64_create_window(&g_desktop_session, 1u, "OpenWindows Taskbar",
                             0, height - 40, width, 40);
        owwm64_create_window(&g_desktop_session, 2u, "OpenWindows Terminal (owsh)",
                             120, 100, 800, 500);
        owwm64_create_window(&g_desktop_session, 3u, "System Activity Monitor",
                             width - 460, 100, 420, 360);
    }

    g_session_running = true;

    /* 3. Run compositor loop (simulated tick in freestanding test) */
    owwm64_set_focus(&g_desktop_session, safe_mode ? 1u : 2u);

    /* Report what the session holds, window by window, so the boot log shows
     * the composed desktop even without a framebuffer to paint it on. */
    owx_puts("startwm: session up with ");
    owx_put_u64(g_desktop_session.window_count);
    owx_puts(" window(s)\r\n");
    for (i = 0; i < g_desktop_session.window_count; i++) {
        const owwm_window_t *w = &g_desktop_session.windows[i];

        owx_puts("  [");
        owx_put_u64(w->window_id);
        owx_puts("] '");
        owx_puts(w->title);
        owx_puts("' at (");
        owx_put_i64(w->bounds.x);
        owx_puts(", ");
        owx_put_i64(w->bounds.y);
        owx_puts(") ");
        owx_put_i64(w->bounds.width);
        owx_puts("x");
        owx_put_i64(w->bounds.height);
        owx_puts(w->is_focused ? " [focus]\r\n" : "\r\n");
    }
    owx_puts("startwm: compositor focus is window ");
    owx_put_u64(g_desktop_session.focused_window_id);
    owx_puts("\r\n");

    return 0;
}

static void startwm_stop_session(void)
{
    if (g_session_running) {
        owwm64_destroy_window(&g_desktop_session, g_desktop_session.focused_window_id);
        g_session_running = false;
        owx_puts("startwm: session stopped\r\n");
    } else {
        owx_puts("startwm: no session to stop\r\n");
    }
}

/* ── CLI Entry Point ──────────────────────────────────────────── */

int startwm_main(int argc, const char *const *argv)
{
    int32_t screen_w = DEFAULT_SCREEN_WIDTH;
    int32_t screen_h = DEFAULT_SCREEN_HEIGHT;
    bool    safe_mode = false;

    if (argc > 1 && argv) {
        if (str_equals(argv[1], "--help") || str_equals(argv[1], "-h")) {
            owx_puts("startwm: OpenWindows desktop session launcher (version ");
            owx_puts(STARTWM_VERSION);
            owx_puts(")\r\n");
            owx_puts("usage: startwm [--safe] [--res WxH] [--status] [--stop] "
                     "[--version]\r\n");
            return 0;
        }
        if (str_equals(argv[1], "--version") || str_equals(argv[1], "-v")) {
            owx_puts("startwm ");
            owx_puts(STARTWM_VERSION);
            owx_puts("\r\n");
            return 0;
        }
        if (str_equals(argv[1], "--stop")) {
            startwm_stop_session();
            return 0;
        }
        if (str_equals(argv[1], "--status")) {
            owx_puts(g_session_running ? "startwm: session running\r\n"
                                       : "startwm: no session\r\n");
            return g_session_running ? 0 : 3;
        }
        if (str_equals(argv[1], "--safe")) {
            safe_mode = true;
        }
        if (str_equals(argv[1], "--res") && argc > 2 && argv[2]) {
            if (str_equals(argv[2], "1024x768")) {
                screen_w = 1024;
                screen_h = 768;
            } else if (str_equals(argv[2], "800x600")) {
                screen_w = 800;
                screen_h = 600;
            } else {
                owx_puts("startwm: unsupported resolution '");
                owx_puts(argv[2]);
                owx_puts("'; using the default\r\n");
            }
        }
    }

    return startwm_launch_session(screen_w, screen_h, safe_mode);
}
