/*
 * owgfx64.c - OpenWindows Graphics Primitives Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owgfx64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owgfx64_ready = false;

/* ── Internal helpers ─────────────────────────────────────────── */

/* Bytes occupied by one pixel of the given depth, 0 when unsupported. */
static size_t owgfx_bpp_bytes(uint8_t bpp)
{
    switch (bpp) {
    case 8:  return 1;
    case 16: return 2;
    case 24: return 3;
    case 32: return 4;
    default: return 0;
    }
}

static bool owgfx_surface_valid(const owgfx_surface_t *fb)
{
    if (!owgfx64_ready || !fb || !fb->pixels) return false;
    if (fb->width <= 0 || fb->height <= 0 || fb->stride <= 0) return false;
    if (owgfx_bpp_bytes(fb->bpp) == 0) return false;
    return true;
}

/* Store a packed color at (x, y) with no bounds checking. */
static void owgfx_store_pixel(uint8_t *pixels, int32_t stride, uint8_t bpp,
                              int32_t x, int32_t y, owgfx_color_t color)
{
    uint8_t *p = pixels + ((size_t)y * (size_t)stride) +
                 ((size_t)x * owgfx_bpp_bytes(bpp));

    switch (bpp) {
    case 8:
        /* 8-bpp surfaces are treated as an indexed palette. */
        p[0] = (uint8_t)(color & 0xFFu);
        break;
    case 16: {
        uint16_t v = (uint16_t)((((color >> 19) & 0x001Fu) << 11) |
                                (((color >> 10) & 0x003Fu) << 5)  |
                                 ((color >> 3)  & 0x001Fu));
        p[0] = (uint8_t)(v & 0x00FFu);
        p[1] = (uint8_t)((v >> 8) & 0x00FFu);
        break;
    }
    case 24:
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        p[2] = (uint8_t)((color >> 16) & 0xFFu);
        break;
    case 32:
        p[0] = (uint8_t)(color & 0xFFu);
        p[1] = (uint8_t)((color >> 8) & 0xFFu);
        p[2] = (uint8_t)((color >> 16) & 0xFFu);
        p[3] = 0xFFu;
        break;
    default:
        break;
    }
}

/* Read a pixel as 0x00RRGGBB with no bounds checking. */
static owgfx_color_t owgfx_read_pixel(const uint8_t *pixels, int32_t stride,
                                      uint8_t bpp, int32_t x, int32_t y)
{
    const uint8_t *p = pixels + ((size_t)y * (size_t)stride) +
                       ((size_t)x * owgfx_bpp_bytes(bpp));

    switch (bpp) {
    case 8:
        return (owgfx_color_t)p[0];
    case 16: {
        uint16_t v = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
        uint32_t r = ((uint32_t)((v >> 11) & 0x001Fu) * 255u) / 31u;
        uint32_t g = ((uint32_t)((v >> 5)  & 0x003Fu) * 255u) / 63u;
        uint32_t b = ((uint32_t)(v & 0x001Fu) * 255u) / 31u;
        return (r << 16) | (g << 8) | b;
    }
    case 24:
    case 32:
        return ((owgfx_color_t)p[2] << 16) |
               ((owgfx_color_t)p[1] << 8)  |
                (owgfx_color_t)p[0];
    default:
        return 0;
    }
}

/* Shared blit core. When `keyed` is true, source pixels equal to `key`
 * are left transparent. */
static void owgfx_blit_impl(owgfx_surface_t *fb, int32_t x, int32_t y,
                            const owgfx_bitmap_t *src, bool keyed,
                            owgfx_color_t key)
{
    if (!owgfx_surface_valid(fb)) return;
    if (!src || !src->pixels) return;
    if (src->width <= 0 || src->height <= 0 || src->stride <= 0) return;
    if (owgfx_bpp_bytes(src->bpp) == 0) return;

    for (int32_t sy = 0; sy < src->height; ++sy) {
        int32_t dy = y + sy;
        if (dy < 0 || dy >= fb->height) continue;

        for (int32_t sx = 0; sx < src->width; ++sx) {
            int32_t dx = x + sx;
            if (dx < 0 || dx >= fb->width) continue;

            owgfx_color_t c = owgfx_read_pixel(src->pixels, src->stride,
                                               src->bpp, sx, sy);
            if (keyed && c == key) continue;

            owgfx_store_pixel(fb->pixels, fb->stride, fb->bpp, dx, dy, c);
        }
    }
}

/* ── Implementation ──────────────────────────────────────────── */

bool owgfx_init(void)
{
    owgfx64_ready = true;
    return owgfx64_ready;
}

bool owgfx_put_pixel(owgfx_surface_t *fb, int32_t x, int32_t y,
                     owgfx_color_t color)
{
    if (!owgfx_surface_valid(fb)) return false;
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height) return false;

    owgfx_store_pixel(fb->pixels, fb->stride, fb->bpp, x, y, color);
    return true;
}

owgfx_color_t owgfx_get_pixel(const owgfx_surface_t *fb, int32_t x, int32_t y)
{
    if (!owgfx64_ready || !fb || !fb->pixels) return 0;
    if (x < 0 || x >= fb->width || y < 0 || y >= fb->height) return 0;
    if (owgfx_bpp_bytes(fb->bpp) == 0) return 0;

    return owgfx_read_pixel(fb->pixels, fb->stride, fb->bpp, x, y);
}

void owgfx_draw_line(owgfx_surface_t *fb, int32_t x0, int32_t y0,
                     int32_t x1, int32_t y1, owgfx_color_t color)
{
    if (!owgfx_surface_valid(fb)) return;

    int64_t dx = (x1 > x0) ? (int64_t)(x1 - x0) : (int64_t)(x0 - x1);
    int64_t dy = (y1 > y0) ? (int64_t)(y1 - y0) : (int64_t)(y0 - y1);
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int64_t err = dx - dy;
    int32_t cx = x0;
    int32_t cy = y0;

    for (;;) {
        (void)owgfx_put_pixel(fb, cx, cy, color);
        if (cx == x1 && cy == y1) break;

        int64_t e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 <  dx) { err += dx; cy += sy; }
    }
}

void owgfx_draw_rect(owgfx_surface_t *fb, int32_t x, int32_t y,
                     int32_t w, int32_t h, owgfx_color_t color)
{
    if (!owgfx_surface_valid(fb)) return;
    if (w <= 0 || h <= 0) return;

    int32_t x1 = x + w - 1;
    int32_t y1 = y + h - 1;

    owgfx_draw_line(fb, x,  y,  x1, y,  color);
    owgfx_draw_line(fb, x,  y1, x1, y1, color);
    owgfx_draw_line(fb, x,  y,  x,  y1, color);
    owgfx_draw_line(fb, x1, y,  x1, y1, color);
}

void owgfx_draw_circle(owgfx_surface_t *fb, int32_t cx, int32_t cy,
                       int32_t radius, owgfx_color_t color)
{
    if (!owgfx_surface_valid(fb)) return;
    if (radius < 0) return;

    int32_t x = radius;
    int32_t y = 0;
    int64_t err = 1 - (int64_t)radius;

    while (x >= y) {
        (void)owgfx_put_pixel(fb, cx + x, cy + y, color);
        (void)owgfx_put_pixel(fb, cx + y, cy + x, color);
        (void)owgfx_put_pixel(fb, cx - y, cy + x, color);
        (void)owgfx_put_pixel(fb, cx - x, cy + y, color);
        (void)owgfx_put_pixel(fb, cx - x, cy - y, color);
        (void)owgfx_put_pixel(fb, cx - y, cy - x, color);
        (void)owgfx_put_pixel(fb, cx + y, cy - x, color);
        (void)owgfx_put_pixel(fb, cx + x, cy - y, color);

        ++y;
        if (err < 0) {
            err += 2 * (int64_t)y + 1;
        } else {
            --x;
            err += 2 * ((int64_t)y - (int64_t)x) + 1;
        }
    }
}

void owgfx_fill_rect(owgfx_surface_t *fb, int32_t x, int32_t y,
                     int32_t w, int32_t h, owgfx_color_t color)
{
    if (!owgfx_surface_valid(fb)) return;
    if (w <= 0 || h <= 0) return;

    /* Clip the requested rectangle against the surface. */
    int32_t x0 = (x < 0) ? 0 : x;
    int32_t y0 = (y < 0) ? 0 : y;
    int32_t x1 = x + w;
    int32_t y1 = y + h;
    if (x1 > fb->width)  x1 = fb->width;
    if (y1 > fb->height) y1 = fb->height;
    if (x0 >= x1 || y0 >= y1) return;

    for (int32_t py = y0; py < y1; ++py) {
        for (int32_t px = x0; px < x1; ++px) {
            owgfx_store_pixel(fb->pixels, fb->stride, fb->bpp, px, py, color);
        }
    }
}

void owgfx_blit_bitmap(owgfx_surface_t *fb, int32_t x, int32_t y,
                       const owgfx_bitmap_t *src)
{
    owgfx_blit_impl(fb, x, y, src, false, 0);
}

void owgfx_blit_bitmap_keyed(owgfx_surface_t *fb, int32_t x, int32_t y,
                             const owgfx_bitmap_t *src, owgfx_color_t key)
{
    owgfx_blit_impl(fb, x, y, src, true, key);
}
