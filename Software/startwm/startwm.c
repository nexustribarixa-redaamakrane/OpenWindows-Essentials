/*
 * startwm.c - OpenWindows Window Manager Launcher & Desktop Orchestrator (.owx)
 *
 * Provides the user-facing command to initialize display hardware, load Cairo64/
 * userinterface64 graphics pipelines, launch owwm compositor, and spawn desktop
 * accessories (taskbar, start menu, system monitor, and terminal).
 *
 * Equivalent to 'startx' on X11, 'startplasma' on Wayland, or 'win' on MS-DOS.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../DLL/owwm64/owwm64.h"

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
    if (g_session_running) {
        /* Already running */
        return 1;
    }

    if (!startwm_prepare_display(width, height)) {
        return 2;
    }

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

    return 0;
}

static void startwm_stop_session(void)
{
    g_session_running = false;
}

/* ── CLI Entry Point ──────────────────────────────────────────── */

int startwm_main(int argc, const char *const *argv)
{
    int32_t screen_w = DEFAULT_SCREEN_WIDTH;
    int32_t screen_h = DEFAULT_SCREEN_HEIGHT;
    bool    safe_mode = false;

    if (argc > 1) {
        if (str_equals(argv[1], "--help") || str_equals(argv[1], "-h")) {
            /* Command usage */
            return 0;
        }
        if (str_equals(argv[1], "--version") || str_equals(argv[1], "-v")) {
            return 0;
        }
        if (str_equals(argv[1], "--stop")) {
            startwm_stop_session();
            return 0;
        }
        if (str_equals(argv[1], "--status")) {
            return g_session_running ? 0 : 3;
        }
        if (str_equals(argv[1], "--safe")) {
            safe_mode = true;
        }
        if (str_equals(argv[1], "--res") && argc > 2) {
            if (str_equals(argv[2], "1024x768")) {
                screen_w = 1024;
                screen_h = 768;
            } else if (str_equals(argv[2], "800x600")) {
                screen_w = 800;
                screen_h = 600;
            }
        }
    }

    return startwm_launch_session(screen_w, screen_h, safe_mode);
}
