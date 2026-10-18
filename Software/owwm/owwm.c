/*
 * owwm.c - OpenWindows Window Manager Main Process (.owx)
 *
 * Runs the desktop composition loop over the window-management library and
 * reports what it built over the Ring 3 console (Software/owx_gate.h), so a
 * session is observable from the serial log rather than only from pixels.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../DLL/owwm64/owwm64.h"
#include "../owx_gate.h"

static owwm_desktop_t g_desktop;

/* The compositor has no framebuffer to paint from inside a Ring 3 image yet,
 * so every window it holds is reported as text: id, owner, geometry, focus. */
static void owwm_desktop_report(const owwm_desktop_t *desk)
{
    uint32_t i;

    owx_puts("owwm: desktop holds ");
    owx_put_u64(desk->window_count);
    owx_puts(" window(s); focused id ");
    owx_put_u64(desk->focused_window_id);
    owx_puts("\r\n");

    for (i = 0; i < desk->window_count; i++) {
        const owwm_window_t *w = &desk->windows[i];

        owx_puts("  [");
        owx_put_u64(w->window_id);
        owx_puts("] '");
        owx_puts(w->title);
        owx_puts("' pid ");
        owx_put_u64(w->owner_pid);
        owx_puts(" at (");
        owx_put_i64(w->bounds.x);
        owx_puts(", ");
        owx_put_i64(w->bounds.y);
        owx_puts(") ");
        owx_put_i64(w->bounds.width);
        owx_puts("x");
        owx_put_i64(w->bounds.height);
        owx_puts(w->is_focused ? " [focus]\r\n" : "\r\n");
    }

    owx_puts("owwm: cursor at (");
    owx_put_i64(desk->cursor_x);
    owx_puts(", ");
    owx_put_i64(desk->cursor_y);
    owx_puts(desk->cursor_visible ? "), visible\r\n" : "), hidden\r\n");
}

int owwm_main(int argc, const char *const argv[])
{
    (void)argc;
    (void)argv;

    owx_puts("owwm: OpenWindows window manager\r\n");

    owwm64_init(&g_desktop);

    /* Spawn default desktop windows */
    owwm64_create_window(&g_desktop, 1u, "OpenWindows Terminal (owsh)", 100, 100, 640, 400);
    owwm64_create_window(&g_desktop, 1u, "System Monitor (top)", 200, 150, 480, 320);

    owwm_desktop_report(&g_desktop);

    return (g_desktop.window_count == 2u) ? 0 : 1;
}
