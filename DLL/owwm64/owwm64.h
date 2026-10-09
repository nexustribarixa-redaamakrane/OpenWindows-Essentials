/*
 * owwm64.h - OpenWindows Window Management Dynamic Library (.owd)
 *
 * Window frame metrics, z-ordering, clipped dirty-rectangles, and event dispatch.
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWWM64_H
#define OWWM64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWWM_MAX_WINDOWS    32u
#define OWWM_TITLE_MAX      64u

typedef struct {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} owwm_rect_t;

typedef struct {
    uint32_t    window_id;
    uint32_t    owner_pid;
    char        title[OWWM_TITLE_MAX];
    owwm_rect_t bounds;
    uint32_t    flags;
    bool        is_visible;
    bool        is_focused;
    bool        is_minimized;
} owwm_window_t;

typedef struct {
    owwm_window_t windows[OWWM_MAX_WINDOWS];
    uint32_t      window_count;
    uint32_t      focused_window_id;
    int32_t       cursor_x;
    int32_t       cursor_y;
    bool          cursor_visible;
} owwm_desktop_t;

void owwm64_init(owwm_desktop_t *desk);
int32_t owwm64_create_window(owwm_desktop_t *desk, uint32_t pid, const char *title, int32_t x, int32_t y, int32_t w, int32_t h);
bool owwm64_destroy_window(owwm_desktop_t *desk, uint32_t wid);
bool owwm64_set_focus(owwm_desktop_t *desk, uint32_t wid);
void owwm64_update_cursor(owwm_desktop_t *desk, int32_t dx, int32_t dy, int32_t screen_w, int32_t screen_h);
int32_t owwm64_hit_test(const owwm_desktop_t *desk, int32_t x, int32_t y);

#endif /* OWWM64_H */
