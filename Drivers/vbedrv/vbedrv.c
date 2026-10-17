/*
 * vbedrv.c - OpenWindows VESA BIOS Extensions Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "vbedrv.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool          vbedrv_initialized = false;
static vbe_caps_t    vbedrv_caps;
static vbe_mode_info_t vbedrv_modes[VBE_MAX_MODES];
static uint32_t      vbedrv_mode_count = 0u;

static bool          vbedrv_selected_valid = false;
static vbe_mode_info_t vbedrv_selected;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Byte-wise clear of a caller buffer. */
static void ow_memset(void *dst, uint8_t value, size_t len)
{
    uint8_t *p = (uint8_t *)dst;
    size_t i;
    for (i = 0u; i < len; ++i) {
        p[i] = value;
    }
}

/* Pixel format implied by a bit depth. */
static vbe_format_t vbe_format_for_bpp(uint16_t bpp)
{
    switch (bpp) {
    case 8u:  return VBE_FMT_INDEX8;
    case 16u: return VBE_FMT_RGB565;
    case 24u: return VBE_FMT_RGB888;
    case 32u: return VBE_FMT_XRGB8888;
    default:  return VBE_FMT_UNKNOWN;
    }
}

/* Full validation of a mode descriptor against VBE constraints. */
static vbe_status_t vbe_validate_info(const vbe_mode_info_t *info)
{
    uint32_t min_pitch;
    uint32_t mask_bits;

    if (info == NULL) {
        return VBE_ERR_INVALID_PARAM;
    }
    if (info->width == 0u || info->height == 0u) {
        return VBE_ERR_INVALID_PARAM;
    }
    if (info->framebuffer == 0u) {
        return VBE_ERR_INVALID_PARAM;
    }
    if (info->format != vbe_format_for_bpp(info->bpp)) {
        return VBE_ERR_UNSUPPORTED;
    }

    /* Scan line must hold width*bpp bits, rounded up to whole bytes. */
    min_pitch = (((uint32_t)info->width * (uint32_t)info->bpp) + 7u) / 8u;
    if ((uint32_t)info->pitch < min_pitch) {
        return VBE_ERR_INVALID_PARAM;
    }

    /* Channel masks only exist for direct-colour modes and must fit. */
    if (info->bpp != 8u) {
        mask_bits = (uint32_t)info->red_size +
                    (uint32_t)info->green_size +
                    (uint32_t)info->blue_size;
        if (mask_bits > (uint32_t)info->bpp) {
            return VBE_ERR_UNSUPPORTED;
        }
        if (info->red_pos >= info->bpp || info->green_pos >= info->bpp ||
            info->blue_pos >= info->bpp) {
            return VBE_ERR_UNSUPPORTED;
        }
    }
    return VBE_OK;
}

/* Locate a database entry by mode number. Returns index or -1. */
static int32_t vbe_find_mode(uint32_t mode_number)
{
    uint32_t i;
    for (i = 0u; i < vbedrv_mode_count; ++i) {
        if (vbedrv_modes[i].mode_number == mode_number) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Store a validated descriptor, replacing any existing mode number. */
static vbe_status_t vbe_store_mode(const vbe_mode_info_t *info)
{
    int32_t idx = vbe_find_mode(info->mode_number);
    if (idx >= 0) {
        vbedrv_modes[(uint32_t)idx] = *info;
        return VBE_OK;
    }
    if (vbedrv_mode_count >= VBE_MAX_MODES) {
        return VBE_ERR_TABLE_FULL;
    }
    vbedrv_modes[vbedrv_mode_count] = *info;
    vbedrv_mode_count++;
    return VBE_OK;
}

/* ── Implementation ───────────────────────────────────────────── */

vbe_status_t vbe_init(const vbe_caps_t *caps)
{
    if (caps == NULL) {
        return VBE_ERR_INVALID_PARAM;
    }

    ow_memset(&vbedrv_caps, 0, sizeof(vbedrv_caps));
    ow_memset(vbedrv_modes, 0, sizeof(vbedrv_modes));
    ow_memset(&vbedrv_selected, 0, sizeof(vbedrv_selected));

    vbedrv_caps = *caps;
    vbedrv_mode_count = 0u;
    vbedrv_selected_valid = false;
    vbedrv_initialized = true;
    return VBE_OK;
}

vbe_status_t vbe_add_mode(const vbe_mode_info_t *info)
{
    vbe_status_t st;

    if (!vbedrv_initialized) {
        return VBE_ERR_NOT_INITIALIZED;
    }
    st = vbe_validate_info(info);
    if (st != VBE_OK) {
        return st;
    }
    return vbe_store_mode(info);
}

vbe_status_t vbe_set_mode(uint32_t mode_number, const vbe_mode_info_t *info)
{
    vbe_status_t st;
    int32_t idx;

    if (!vbedrv_initialized) {
        return VBE_ERR_NOT_INITIALIZED;
    }

    if (info != NULL) {
        /* Caller-supplied descriptor: validate, register, then select. */
        st = vbe_validate_info(info);
        if (st != VBE_OK) {
            return st;
        }
        st = vbe_store_mode(info);
        if (st != VBE_OK) {
            return st;
        }
        vbedrv_selected = *info;
        vbedrv_selected_valid = true;
        return VBE_OK;
    }

    /* No descriptor: select a previously registered mode. */
    idx = vbe_find_mode(mode_number);
    if (idx < 0) {
        return VBE_ERR_NO_SUCH_MODE;
    }
    vbedrv_selected = vbedrv_modes[(uint32_t)idx];
    vbedrv_selected_valid = true;
    return VBE_OK;
}

volatile uint8_t *vbe_get_framebuffer(void)
{
    if (!vbedrv_initialized || !vbedrv_selected_valid) {
        return NULL;
    }
    return (volatile uint8_t *)(uintptr_t)vbedrv_selected.framebuffer;
}

bool vbe_get_info(vbe_mode_info_t *out_info)
{
    if (out_info == NULL) {
        return false;
    }
    if (!vbedrv_initialized || !vbedrv_selected_valid) {
        return false;
    }
    *out_info = vbedrv_selected;
    return true;
}

uint32_t vbe_mode_count(void)
{
    return vbedrv_mode_count;
}
