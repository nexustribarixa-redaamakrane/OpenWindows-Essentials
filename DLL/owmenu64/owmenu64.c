/*
 * owmenu64.c - OpenWindows Menu System Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owmenu64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owmenu64_ready = false;
static owmenu_t owmenu64_menus[OWMENU_MAX_MENUS];

/* ── Internal helpers ─────────────────────────────────────────── */

static void owmenu_copy_string(char *dst, const char *src)
{
    uint32_t i = 0u;

    if (src) {
        for (i = 0u; i + 1u < OWMENU_LABEL_MAX && src[i] != '\0'; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

static uint32_t owmenu_strlen(const char *s)
{
    uint32_t n = 0u;

    if (s) {
        while (s[n] != '\0') ++n;
    }
    return n;
}

/* Resolve a handle to a live menu, or NULL. */
static owmenu_t *owmenu_find(int32_t handle)
{
    if (handle < 0 || handle >= (int32_t)OWMENU_MAX_MENUS) return NULL;
    if (!owmenu64_menus[handle].used) return NULL;
    return &owmenu64_menus[handle];
}

/* Rendered width of a menu, derived from the widest label. */
static int32_t owmenu_menu_width(const owmenu_t *menu)
{
    uint32_t widest = 0u;

    for (uint32_t i = 0u; i < menu->item_count; ++i) {
        if (menu->items[i].separator) continue;
        uint32_t len = owmenu_strlen(menu->items[i].label);
        if (len > widest) widest = len;
    }

    int32_t width = (int32_t)(widest * (uint32_t)OWMENU_CHAR_WIDTH) +
                    (OWMENU_PADDING_X * 2);
    if (width < 64) width = 64;
    return width;
}

/* ── Implementation ──────────────────────────────────────────── */

bool owmenu_init(void)
{
    for (uint32_t i = 0u; i < OWMENU_MAX_MENUS; ++i) {
        owmenu64_menus[i].used = false;
        owmenu64_menus[i].id = 0u;
        owmenu64_menus[i].item_count = 0u;
        owmenu64_menus[i].last_selection = 0u;
        owmenu64_menus[i].title[0] = '\0';
    }
    owmenu64_ready = true;
    return owmenu64_ready;
}

int32_t owmenu_create(const char *title)
{
    if (!owmenu64_ready) return -1;

    for (uint32_t i = 0u; i < OWMENU_MAX_MENUS; ++i) {
        if (owmenu64_menus[i].used) continue;

        owmenu_t *m = &owmenu64_menus[i];
        m->id = i;
        m->item_count = 0u;
        m->last_selection = 0u;
        m->used = true;
        owmenu_copy_string(m->title, title);
        return (int32_t)i;
    }

    return -1;
}

bool owmenu_add_item(int32_t menu, const char *label, uint32_t id,
                     bool enabled)
{
    owmenu_t *m = owmenu_find(menu);
    if (!m) return false;
    if (m->item_count >= OWMENU_MAX_ITEMS) return false;

    owmenu_item_t *item = &m->items[m->item_count];
    owmenu_copy_string(item->label, label);
    item->id = id;
    item->enabled = enabled;
    item->checked = false;
    item->separator = false;
    ++m->item_count;
    return true;
}

bool owmenu_add_separator(int32_t menu)
{
    owmenu_t *m = owmenu_find(menu);
    if (!m) return false;
    if (m->item_count >= OWMENU_MAX_ITEMS) return false;

    owmenu_item_t *item = &m->items[m->item_count];
    item->label[0] = '\0';
    item->id = 0u;
    item->enabled = false;
    item->checked = false;
    item->separator = true;
    ++m->item_count;
    return true;
}

bool owmenu_hit_test(int32_t menu, int32_t origin_x, int32_t origin_y,
                     int32_t px, int32_t py, uint32_t *out_item_id)
{
    owmenu_t *m = owmenu_find(menu);
    if (!m) return false;

    int32_t width = owmenu_menu_width(m);
    if (px < origin_x || px >= origin_x + width) return false;

    for (uint32_t i = 0u; i < m->item_count; ++i) {
        const owmenu_item_t *item = &m->items[i];
        if (item->separator) continue;

        int32_t y0 = origin_y + (int32_t)(i * (uint32_t)OWMENU_ITEM_HEIGHT);
        int32_t y1 = y0 + OWMENU_ITEM_HEIGHT;
        if (py < y0 || py >= y1) continue;

        /* A disabled row absorbs the click but yields no selection. */
        if (!item->enabled) return false;

        if (out_item_id) *out_item_id = item->id;
        return true;
    }

    return false;
}

bool owmenu_show(int32_t menu, int32_t origin_x, int32_t origin_y,
                 int32_t pointer_x, int32_t pointer_y,
                 owmenu_show_fn hook, void *ctx, uint32_t *out_selection)
{
    owmenu_t *m = owmenu_find(menu);
    if (!m) return false;

    uint32_t selection = 0u;
    bool selected;

    if (hook) {
        selected = hook(ctx, m, origin_x, origin_y, &selection);
    } else {
        selected = owmenu_hit_test(menu, origin_x, origin_y,
                                   pointer_x, pointer_y, &selection);
    }

    m->last_selection = selected ? selection : 0u;
    if (out_selection) *out_selection = m->last_selection;
    return selected;
}

bool owmenu_destroy(int32_t menu)
{
    owmenu_t *m = owmenu_find(menu);
    if (!m) return false;

    m->used = false;
    m->item_count = 0u;
    m->last_selection = 0u;
    m->title[0] = '\0';
    return true;
}

uint32_t owmenu_get_selection(int32_t menu)
{
    owmenu_t *m = owmenu_find(menu);
    if (!m) return 0u;
    return m->last_selection;
}

uint32_t owmenu_get_item_count(int32_t menu)
{
    owmenu_t *m = owmenu_find(menu);
    if (!m) return 0u;
    return m->item_count;
}
