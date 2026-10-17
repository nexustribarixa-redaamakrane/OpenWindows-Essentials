/*
 * fbdev.c - OpenWindows Framebuffer Device Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "fbdev.h"

/* ── Internal state ───────────────────────────────────────────── */

static volatile uint8_t *fbdev_base   = NULL;
static fbdev_format_t    fbdev_format = FBDEV_FMT_INDEX8;
static uint32_t          fbdev_width  = 0u;
static uint32_t          fbdev_height = 0u;
static uint32_t          fbdev_pitch  = 0u;
static uint16_t          fbdev_bpp    = 0u;
static bool              fbdev_initialized = false;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Map a bit depth onto a supported pixel format. */
static bool fbdev_format_for_bpp(uint16_t bpp, fbdev_format_t *out)
{
    switch (bpp) {
    case 8u:  *out = FBDEV_FMT_INDEX8;   return true;
    case 16u: *out = FBDEV_FMT_RGB565;   return true;
    case 24u: *out = FBDEV_FMT_BGR888;   return true;
    case 32u: *out = FBDEV_FMT_XRGB8888; return true;
    default:  return false;
    }
}

/* Number of bytes occupied by one pixel at the given depth. */
static uint32_t fbdev_bytes_per_pixel(uint16_t bpp)
{
    return ((uint32_t)bpp + 7u) / 8u;
}

/* Encode a 0xRRGGBB colour into the framebuffer's native layout. */
static void fbdev_encode(uint16_t bpp, fbdev_color_t color, uint8_t *dst)
{
    uint8_t r = (uint8_t)((color >> 16) & 0xFFu);
    uint8_t g = (uint8_t)((color >> 8) & 0xFFu);
    uint8_t b = (uint8_t)(color & 0xFFu);

    switch (bpp) {
    case 8u:
        /* Palettised: the low byte selects the palette entry. */
        dst[0] = (uint8_t)(color & 0xFFu);
        break;
    case 16u: {
        uint16_t v = (uint16_t)(((uint16_t)(r >> 3) << 11) |
                                ((uint16_t)(g >> 2) << 5) |
                                ((uint16_t)(b >> 3)));
        dst[0] = (uint8_t)(v & 0xFFu);
        dst[1] = (uint8_t)((v >> 8) & 0xFFu);
        break;
    }
    case 24u:
        dst[0] = b;
        dst[1] = g;
        dst[2] = r;
        break;
    case 32u:
        dst[0] = b;
        dst[1] = g;
        dst[2] = r;
        dst[3] = 0u;
        break;
    default:
        break;
    }
}

/* Decode a native-layout pixel back into a 0xRRGGBB colour. */
static fbdev_color_t fbdev_decode(uint16_t bpp, const uint8_t *src)
{
    switch (bpp) {
    case 8u:
        return (fbdev_color_t)src[0];
    case 16u: {
        uint16_t v = (uint16_t)((uint16_t)src[0] | ((uint16_t)src[1] << 8));
        uint8_t r = (uint8_t)((v >> 11) & 0x1Fu);
        uint8_t g = (uint8_t)((v >> 5) & 0x3Fu);
        uint8_t b = (uint8_t)(v & 0x1Fu);
        r = (uint8_t)((r << 3) | (r >> 2));
        g = (uint8_t)((g << 2) | (g >> 4));
        b = (uint8_t)((b << 3) | (b >> 2));
        return FBDEV_RGB(r, g, b);
    }
    case 24u:
        return FBDEV_RGB(src[2], src[1], src[0]);
    case 32u:
        return FBDEV_RGB(src[2], src[1], src[0]);
    default:
        return 0u;
    }
}

/* Write one native pixel at a valid (already bounds-checked) location. */
static void fbdev_store_pixel(uint32_t x, uint32_t y, fbdev_color_t color)
{
    uint8_t *row = (uint8_t *)(fbdev_base + ((size_t)y * fbdev_pitch));
    fbdev_encode(fbdev_bpp, color, row + ((size_t)x * fbdev_bytes_per_pixel(fbdev_bpp)));
}

/* Clip a rectangle to the surface; returns false when fully outside. */
static bool fbdev_clip_rect(fbdev_rect_t *rect)
{
    int32_t x0 = rect->x;
    int32_t y0 = rect->y;
    int32_t x1 = rect->x + rect->width;
    int32_t y1 = rect->y + rect->height;

    if (x0 < 0) { x0 = 0; }
    if (y0 < 0) { y0 = 0; }
    if (x1 > (int32_t)fbdev_width)  { x1 = (int32_t)fbdev_width; }
    if (y1 > (int32_t)fbdev_height) { y1 = (int32_t)fbdev_height; }
    if (x1 <= x0 || y1 <= y0) {
        return false;
    }
    rect->x = x0;
    rect->y = y0;
    rect->width = x1 - x0;
    rect->height = y1 - y0;
    return true;
}

/* ── Implementation ───────────────────────────────────────────── */

bool fbdev_init(volatile uint8_t *base, const fbdev_geometry_t *geo)
{
    fbdev_format_t fmt;
    uint32_t min_pitch;

    if (base == NULL || geo == NULL) {
        return false;
    }
    if (geo->width == 0u || geo->height == 0u) {
        return false;
    }
    if (!fbdev_format_for_bpp(geo->bpp, &fmt)) {
        return false;
    }

    min_pitch = geo->width * fbdev_bytes_per_pixel(geo->bpp);
    if (geo->pitch < min_pitch) {
        return false;
    }

    fbdev_base   = base;
    fbdev_format = fmt;
    fbdev_width  = geo->width;
    fbdev_height = geo->height;
    fbdev_pitch  = geo->pitch;
    fbdev_bpp    = geo->bpp;
    fbdev_initialized = true;
    return true;
}

bool fbdev_putpixel(int32_t x, int32_t y, fbdev_color_t color)
{
    if (!fbdev_initialized) {
        return false;
    }
    if (x < 0 || y < 0) {
        return false;
    }
    if ((uint32_t)x >= fbdev_width || (uint32_t)y >= fbdev_height) {
        return false;
    }
    fbdev_store_pixel((uint32_t)x, (uint32_t)y, color);
    return true;
}

uint32_t fbdev_fill_rect(const fbdev_rect_t *rect, fbdev_color_t color)
{
    fbdev_rect_t r;
    uint32_t written = 0u;
    uint32_t yy;

    if (!fbdev_initialized || rect == NULL) {
        return 0u;
    }
    if (rect->width <= 0 || rect->height <= 0) {
        return 0u;
    }

    r = *rect;
    if (!fbdev_clip_rect(&r)) {
        return 0u;
    }

    for (yy = (uint32_t)r.y; yy < (uint32_t)(r.y + r.height); ++yy) {
        uint32_t xx;
        for (xx = (uint32_t)r.x; xx < (uint32_t)(r.x + r.width); ++xx) {
            fbdev_store_pixel(xx, yy, color);
            written++;
        }
    }
    return written;
}

uint32_t fbdev_blit(int32_t dst_x, int32_t dst_y, const fbdev_bitmap_t *src)
{
    uint32_t written = 0u;
    int32_t sy_start = 0;
    int32_t sx_start = 0;
    int32_t copy_w;
    int32_t copy_h;
    int32_t row;
    uint32_t src_bpp;

    if (!fbdev_initialized || src == NULL || src->pixels == NULL) {
        return 0u;
    }
    if (src->width == 0u || src->height == 0u) {
        return 0u;
    }

    src_bpp = fbdev_bytes_per_pixel(src->bpp);

    /* Intersect the destination rectangle with the surface. */
    copy_w = (int32_t)src->width;
    copy_h = (int32_t)src->height;
    if (dst_x < 0) {
        sx_start = -dst_x;
        copy_w -= sx_start;
        dst_x = 0;
    }
    if (dst_y < 0) {
        sy_start = -dst_y;
        copy_h -= sy_start;
        dst_y = 0;
    }
    if ((uint32_t)dst_x + (uint32_t)copy_w > fbdev_width) {
        copy_w = (int32_t)fbdev_width - dst_x;
    }
    if ((uint32_t)dst_y + (uint32_t)copy_h > fbdev_height) {
        copy_h = (int32_t)fbdev_height - dst_y;
    }
    if (copy_w <= 0 || copy_h <= 0) {
        return 0u;
    }

    for (row = 0; row < copy_h; ++row) {
        uint32_t sy = (uint32_t)(sy_start + row);
        const uint8_t *srow = src->pixels + ((size_t)sy * src->pitch);
        int32_t col;
        for (col = 0; col < copy_w; ++col) {
            uint32_t sx = (uint32_t)(sx_start + col);
            fbdev_color_t c = fbdev_decode(src->bpp,
                                           srow + ((size_t)sx * src_bpp));
            fbdev_store_pixel((uint32_t)(dst_x + col),
                              (uint32_t)(dst_y + row), c);
            written++;
        }
    }
    return written;
}

void fbdev_clear(fbdev_color_t color)
{
    fbdev_rect_t full;
    if (!fbdev_initialized) {
        return;
    }
    full.x = 0;
    full.y = 0;
    full.width = (int32_t)fbdev_width;
    full.height = (int32_t)fbdev_height;
    (void)fbdev_fill_rect(&full, color);
}

bool fbdev_get_info(fbdev_info_t *out_info)
{
    if (out_info == NULL || !fbdev_initialized) {
        return false;
    }
    out_info->base       = fbdev_base;
    out_info->width      = fbdev_width;
    out_info->height     = fbdev_height;
    out_info->pitch      = fbdev_pitch;
    out_info->bpp        = fbdev_bpp;
    out_info->format     = fbdev_format;
    out_info->size_bytes = (size_t)fbdev_pitch * (size_t)fbdev_height;
    return true;
}
