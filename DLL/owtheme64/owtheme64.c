/*
 * owtheme64.c - OpenWindows Theme Engine Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owtheme64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owtheme64_ready = false;
static bool owtheme64_pending_valid = false;
static uint32_t owtheme64_revision = 0u;
static owtheme_descriptor_t owtheme64_active;
static owtheme_descriptor_t owtheme64_pending;

/* Built-in dark theme, applied by owtheme_init(). */
static const owtheme_descriptor_t owtheme64_default = {
    "ow-default-dark",
    "owfont-default",
    16u,
    0u,
    {
        0x001E1E1Eu,  /* WINDOW_BG   */
        0x00E0E0E0u,  /* WINDOW_FG   */
        0x002D2D30u,  /* TITLE_BG    */
        0x00FFFFFFu,  /* TITLE_FG    */
        0x003C3C3Cu,  /* BUTTON_BG   */
        0x00F0F0F0u,  /* BUTTON_FG   */
        0x000078D4u,  /* ACCENT      */
        0x00555555u,  /* BORDER      */
        0x002B2B2Bu,  /* MENU_BG     */
        0x00DDDDDDu,  /* MENU_FG     */
        0x00333333u,  /* TOOLTIP_BG  */
        0x00FFFFFFu,  /* TOOLTIP_FG  */
        0x00000000u,  /* reserved    */
        0x00000000u,
        0x00000000u,
        0x00000000u
    }
};

/* ── Internal helpers ─────────────────────────────────────────── */

/* Copy a name field, always NUL-terminating. */
static void owtheme_copy_name(char *dst, const char *src)
{
    uint32_t i = 0;

    if (src) {
        for (i = 0; i < OWTHEME_NAME_MAX - 1u && src[i] != '\0'; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

/* Copy a whole descriptor with explicit (non-struct-assignment) copies so
 * padding is deterministic. */
static void owtheme_copy_descriptor(owtheme_descriptor_t *dst,
                                    const owtheme_descriptor_t *src)
{
    owtheme_copy_name(dst->name, src->name);
    owtheme_copy_name(dst->font, src->font);
    dst->font_size = src->font_size;
    dst->flags = src->flags;
    for (uint32_t i = 0; i < OWTHEME_MAX_ROLES; ++i) {
        dst->colors[i] = src->colors[i];
    }
}

/* ── Implementation ──────────────────────────────────────────── */

bool owtheme_init(void)
{
    owtheme_copy_descriptor(&owtheme64_active, &owtheme64_default);
    owtheme_copy_descriptor(&owtheme64_pending, &owtheme64_default);
    owtheme64_pending_valid = true;
    owtheme64_revision = 1u;
    owtheme64_ready = true;
    return owtheme64_ready;
}

bool owtheme_load(const owtheme_descriptor_t *desc)
{
    if (!owtheme64_ready) return false;
    if (!desc) return false;
    if (desc->font_size == 0u) return false;

    owtheme_copy_descriptor(&owtheme64_pending, desc);
    owtheme64_pending_valid = true;
    return true;
}

bool owtheme_apply(void)
{
    if (!owtheme64_ready || !owtheme64_pending_valid) return false;

    owtheme_copy_descriptor(&owtheme64_active, &owtheme64_pending);
    ++owtheme64_revision;
    return true;
}

uint32_t owtheme_get_color(owtheme_role_t role)
{
    if (!owtheme64_ready) return 0u;
    if ((uint32_t)role >= (uint32_t)OWTHEME_MAX_ROLES) return 0u;
    return owtheme64_active.colors[(uint32_t)role];
}

const char *owtheme_get_font(void)
{
    return owtheme64_active.font;
}

uint32_t owtheme_get_font_size(void)
{
    return owtheme64_active.font_size;
}

uint32_t owtheme_get_revision(void)
{
    return owtheme64_revision;
}
