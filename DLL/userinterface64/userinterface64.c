/*
 * userinterface64.c - userinterface64.owd implementation (compositor)
 *
 * Light compositor over the genuine cairo engine exported by cairo64.owd.
 * Every cairo_* symbol comes from the cairo64 ABI mirror
 * (Extensions/cairo64.h): with CAIRO_WIN32_STATIC_BUILD it binds the
 * vendor archives (hosted tests); without, __declspec(dllimport) binds
 * cairo64.owd at module link.
 *
 * A window is an ARGB32 image surface (the back buffer). Clear and present
 * walk through normal cairo gstate; present composites with source-over
 * onto an arbitrary target surface, clipped to the target bounds. All
 * allocation is cairo64's (no heap of its own).
 *
 * C99 freestanding strict profile.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "userinterface64.h"   /* Extensions ABI mirror */
#include "owd_format.h"
#include "owc_format.h"
#include "kernel64.h"

/* OWD1 binary header metadata (documentary; see Extensions/owd_format.h). */
#define USERINTERFACE64_LIB_NAME        "userinterface64.owd"
#define USERINTERFACE64_LIB_TYPE        OWD_LIBTYPE_USER    /* userland UI */
#define USERINTERFACE64_TARGET_ARCH     0x02u
#define USERINTERFACE64_ALIGNMENT_LOG2  4u
#define USERINTERFACE64_INIT_FLAGS      OWC_INIT_REQUIRES_BANC

#define USERINTERFACE64_LIB_VERSION     "userinterface64.owd 1.0.0"

static const uint8_t ui64_ident[] = USERINTERFACE64_LIB_VERSION;

/* ------------------------------------------------------------------ */
/*  Module lifecycle                                                   */
/* ------------------------------------------------------------------ */

static bool ui64_initialized = false;

userinterface64_status_t userinterface64_module_init(void)
{
    if (ui64_initialized) {
        return UI64_OK;
    }
    if (k64_initialize_api() != K64_OK) {
        return UI64_BAN_K64_BOOT;
    }
    ui64_initialized = true;
    return UI64_OK;
}

userinterface64_status_t userinterface64_module_shutdown(void)
{
    if (!ui64_initialized) {
        return UI64_ERR_UNINITIALIZED;
    }
    ui64_initialized = false;
    return UI64_OK;
}

uint32_t userinterface64_abi_version(void)
{
    return (uint32_t)((1u * 10000u) + (0u * 100u) + 0u);
}

uint16_t userinterface64_abi_major(void)
{
    return 1u;
}

uint16_t userinterface64_abi_minor(void)
{
    return 0u;
}

const uint8_t *userinterface64_ident(void)
{
    return ui64_ident;
}

#if defined(_WIN32)
unsigned long __stdcall owui64_dll_ep(void)
{
    return 1ul;
}
#endif

/* ------------------------------------------------------------------ */
/*  Window                                                             */
/* ------------------------------------------------------------------ */

struct userinterface64_window {
    uint32_t         w;
    uint32_t         h;
    cairo_surface_t *surf;   /* ARGB32 back buffer */
    bool             live;
};

/*
 * Tiny fixed arena for window structs: no heap of its own. The back
 * buffers themselves are cairo surfaces (allocated by cairo64's allocator);
 * this struct is only 24 bytes so a static pool suffices.
 */
#define UI64_MAX_WINDOWS  32u
static userinterface64_window_t ui64_arena[UI64_MAX_WINDOWS];
static uint32_t ui64_arena_map;   /* bit i set = slot i live */

static userinterface64_window_t *ui64_slot_alloc(void)
{
    uint32_t i;
    for (i = 0u; i < UI64_MAX_WINDOWS; i++) {
        if (!(ui64_arena_map & (1u << i))) {
            ui64_arena_map |= (1u << i);
            ui64_arena[i].live = true;
            return &ui64_arena[i];
        }
    }
    return NULL;
}

static void ui64_slot_free(userinterface64_window_t *win)
{
    uint32_t i = (uint32_t)(win - &ui64_arena[0]);
    ui64_arena[i].live = false;
    ui64_arena_map &= ~(1u << i);
}

static void ui64_fill_argb(userinterface64_window_t *win, uint32_t argb)
{
    cairo_t *cr = cairo_create(win->surf);
    cairo_set_source_rgba(cr,
                          (double)((argb >> 16u) & 0xFFu) / 255.0,
                          (double)((argb >>  8u) & 0xFFu) / 255.0,
                          (double)( argb        & 0xFFu) / 255.0,
                          (double)((argb >> 24u) & 0xFFu) / 255.0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_flush(win->surf);
}

userinterface64_window_t *
userinterface64_window_create(uint32_t w, uint32_t h, uint32_t bg_argb)
{
    userinterface64_window_t *win;
    cairo_surface_t *surf;

    if (w == 0u || h == 0u || !ui64_initialized) {
        return NULL;
    }

    surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, (int)w, (int)h);
    if (cairo_surface_status(surf) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(surf);
        return NULL;
    }

    win = ui64_slot_alloc();
    if (!win) {
        cairo_surface_destroy(surf);
        return NULL;
    }

    win->w    = w;
    win->h    = h;
    win->surf = surf;

    ui64_fill_argb(win, bg_argb);
    return win;
}

void userinterface64_window_destroy(userinterface64_window_t *win)
{
    if (!win) {
        return;
    }
    cairo_surface_destroy(win->surf);
    ui64_slot_free(win);
}

userinterface64_status_t
userinterface64_window_clear(userinterface64_window_t *win, uint32_t argb)
{
    if (!win) {
        return UI64_ERR_BAD_WINDOW;
    }
    ui64_fill_argb(win, argb);
    return UI64_OK;
}

cairo_surface_t *
userinterface64_window_surface(userinterface64_window_t *win)
{
    if (!win) {
        return NULL;
    }
    return win->surf;
}

userinterface64_status_t
userinterface64_window_present(userinterface64_window_t *win,
                               cairo_surface_t *target,
                               int dx, int dy)
{
    cairo_t *cr;
    int tw, th;
    int ix0, iy0, ix1, iy1;

    if (!win) {
        return UI64_ERR_BAD_WINDOW;
    }
    if (!target || !win->surf) {
        return UI64_ERR_BAD_TARGET;
    }

    tw = cairo_image_surface_get_width(target);
    th = cairo_image_surface_get_height(target);
    if (tw < 0 || th < 0) {
        return UI64_OK;                  /* non-image target: untracked */
    }

    /* Window rect vs target-bounds intersection. */
    ix0 = dx < 0 ? 0 : dx;
    iy0 = dy < 0 ? 0 : dy;
    ix1 = dx + (int)win->w < tw ? dx + (int)win->w : tw;
    iy1 = dy + (int)win->h < th ? dy + (int)win->h : th;

    cr = cairo_create(target);
    if (!cr) {
        return UI64_BAN_CTX_FAULT;
    }

    cairo_set_source_surface(cr, win->surf, (double)dx, (double)dy);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_flush(target);

    if (ix0 >= ix1 || iy0 >= iy1) {
        return UI64_ERR_CLIPPED;         /* fully outside target */
    }
    if (ix1 - ix0 < (int)win->w || iy1 - iy0 < (int)win->h) {
        return UI64_ERR_CLIPPED;         /* partially cut */
    }
    return UI64_OK;
}

uint32_t userinterface64_window_width(const userinterface64_window_t *win)
{
    return win ? win->w : 0u;
}

uint32_t userinterface64_window_height(const userinterface64_window_t *win)
{
    return win ? win->h : 0u;
}