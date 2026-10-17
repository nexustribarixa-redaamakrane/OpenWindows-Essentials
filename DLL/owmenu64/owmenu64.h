/*
 * owmenu64.h - OpenWindows Menu System Library (.owd)
 *
 * Fixed-capacity menu model. Menus are created from a static pool, items
 * are appended with a caller id, and owmenu_show() either renders through
 * an injected hook or resolves a selection by hit-testing a pointer
 * position against the default layout.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWMENU64_H
#define OWMENU64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWMENU_MAX_MENUS   16u  /* menu slots                        */
#define OWMENU_MAX_ITEMS   32u  /* items per menu                    */
#define OWMENU_LABEL_MAX   32u  /* stored label length               */

/* Default layout metrics used for hit-testing. */
#define OWMENU_ITEM_HEIGHT 20    /* row height in pixels             */
#define OWMENU_CHAR_WIDTH   8    /* assumed glyph advance            */
#define OWMENU_PADDING_X   12    /* left/right padding in pixels      */

typedef struct {
    char     label[OWMENU_LABEL_MAX];
    uint32_t id;            /* caller id returned on selection   */
    bool     enabled;
    bool     checked;
    bool     separator;     /* non-selectable divider            */
} owmenu_item_t;

typedef struct {
    uint32_t      id;
    char          title[OWMENU_LABEL_MAX];
    owmenu_item_t items[OWMENU_MAX_ITEMS];
    uint32_t      item_count;
    uint32_t      last_selection;
    bool          used;
} owmenu_t;

/* Rendering hook: draw `menu` at (x, y) and write the chosen item id to
 * *out_item_id. Return true when an item was selected, false to dismiss. */
typedef bool (*owmenu_show_fn)(void *ctx, const owmenu_t *menu,
                               int32_t x, int32_t y, uint32_t *out_item_id);

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize (and clear) the menu pool. */
bool owmenu_init(void);

/* Create a menu and return its handle (>= 0), or -1 when the pool is
 * full. */
int32_t owmenu_create(const char *title);

/* Append an item. Returns false when the menu is invalid or full. */
bool owmenu_add_item(int32_t menu, const char *label, uint32_t id,
                     bool enabled);

/* Append a non-selectable separator. */
bool owmenu_add_separator(int32_t menu);

/* Show the menu at (origin_x, origin_y). With a hook, the hook decides
 * the selection; without one the pointer position (pointer_x, pointer_y)
 * is hit-tested against the default layout. The selected item id is
 * written through `out_selection` (0 when dismissed). */
bool owmenu_show(int32_t menu, int32_t origin_x, int32_t origin_y,
                 int32_t pointer_x, int32_t pointer_y,
                 owmenu_show_fn hook, void *ctx, uint32_t *out_selection);

/* Hit-test a point against the default layout. Returns true and writes
 * the item id when an enabled item was hit. */
bool owmenu_hit_test(int32_t menu, int32_t origin_x, int32_t origin_y,
                     int32_t px, int32_t py, uint32_t *out_item_id);

/* Destroy a menu handle. */
bool owmenu_destroy(int32_t menu);

/* Most recent selection for a menu, 0 when none. */
uint32_t owmenu_get_selection(int32_t menu);

/* Number of items currently in a menu. */
uint32_t owmenu_get_item_count(int32_t menu);

#endif /* OWMENU64_H */
