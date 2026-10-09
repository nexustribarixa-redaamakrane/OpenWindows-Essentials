/*
 * owwm64.c - OpenWindows Window Management Dynamic Library Implementation (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owwm64.h"

void owwm64_init(owwm_desktop_t *desk)
{
    if (!desk) return;
    desk->window_count = 0u;
    desk->focused_window_id = 0u;
    desk->cursor_x = 512;
    desk->cursor_y = 384;
    desk->cursor_visible = true;
    for (size_t i = 0u; i < OWWM_MAX_WINDOWS; ++i) {
        desk->windows[i].window_id = 0u;
        desk->windows[i].is_visible = false;
        desk->windows[i].is_focused = false;
        desk->windows[i].is_minimized = false;
    }
}

int32_t owwm64_create_window(owwm_desktop_t *desk, uint32_t pid, const char *title, int32_t x, int32_t y, int32_t w, int32_t h)
{
    if (!desk) return -1;
    for (size_t i = 0u; i < OWWM_MAX_WINDOWS; ++i) {
        if (desk->windows[i].window_id == 0u) {
            uint32_t wid = (uint32_t)(i + 1u);
            desk->windows[i].window_id = wid;
            desk->windows[i].owner_pid = pid;
            desk->windows[i].bounds.x = x;
            desk->windows[i].bounds.y = y;
            desk->windows[i].bounds.width = w;
            desk->windows[i].bounds.height = h;
            desk->windows[i].is_visible = true;
            desk->windows[i].is_minimized = false;

            size_t ti = 0u;
            if (title) {
                while (ti < OWWM_TITLE_MAX - 1 && title[ti]) {
                    desk->windows[i].title[ti] = title[ti];
                    ti++;
                }
            }
            desk->windows[i].title[ti] = '\0';

            desk->window_count++;
            owwm64_set_focus(desk, wid);
            return (int32_t)wid;
        }
    }
    return -1;
}

bool owwm64_destroy_window(owwm_desktop_t *desk, uint32_t wid)
{
    if (!desk || wid == 0u) return false;
    for (size_t i = 0u; i < OWWM_MAX_WINDOWS; ++i) {
        if (desk->windows[i].window_id == wid) {
            desk->windows[i].window_id = 0u;
            desk->windows[i].is_visible = false;
            desk->windows[i].is_focused = false;
            if (desk->window_count > 0u) desk->window_count--;
            if (desk->focused_window_id == wid) desk->focused_window_id = 0u;
            return true;
        }
    }
    return false;
}

bool owwm64_set_focus(owwm_desktop_t *desk, uint32_t wid)
{
    if (!desk) return false;
    bool found = false;
    for (size_t i = 0u; i < OWWM_MAX_WINDOWS; ++i) {
        if (desk->windows[i].window_id == wid && desk->windows[i].is_visible) {
            desk->windows[i].is_focused = true;
            desk->focused_window_id = wid;
            found = true;
        } else {
            desk->windows[i].is_focused = false;
        }
    }
    return found;
}

void owwm64_update_cursor(owwm_desktop_t *desk, int32_t dx, int32_t dy, int32_t screen_w, int32_t screen_h)
{
    if (!desk) return;
    desk->cursor_x += dx;
    desk->cursor_y += dy;
    if (desk->cursor_x < 0) desk->cursor_x = 0;
    if (desk->cursor_x >= screen_w) desk->cursor_x = screen_w - 1;
    if (desk->cursor_y < 0) desk->cursor_y = 0;
    if (desk->cursor_y >= screen_h) desk->cursor_y = screen_h - 1;
}

int32_t owwm64_hit_test(const owwm_desktop_t *desk, int32_t x, int32_t y)
{
    if (!desk) return -1;
    /* Iterate reverse (top-most z-order) */
    for (int32_t i = (int32_t)OWWM_MAX_WINDOWS - 1; i >= 0; --i) {
        const owwm_window_t *w = &desk->windows[i];
        if (w->window_id != 0u && w->is_visible && !w->is_minimized) {
            if (x >= w->bounds.x && x < w->bounds.x + w->bounds.width &&
                y >= w->bounds.y && y < w->bounds.y + w->bounds.height) {
                return (int32_t)w->window_id;
            }
        }
    }
    return -1;
}
