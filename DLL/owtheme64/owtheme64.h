/*
 * owtheme64.h - OpenWindows Theme Engine Library (.owd)
 *
 * Holds a role-indexed color palette plus a font selection. Themes are
 * loaded into a pending slot and committed with owtheme_apply(), so a
 * half-built descriptor is never visible to the rest of the system.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWTHEME64_H
#define OWTHEME64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWTHEME_MAX_ROLES 16u  /* palette entries per theme       */
#define OWTHEME_NAME_MAX  32u  /* theme / font name length         */

/* Semantic color roles. */
typedef enum {
    OWTHEME_ROLE_WINDOW_BG = 0,
    OWTHEME_ROLE_WINDOW_FG,
    OWTHEME_ROLE_TITLE_BG,
    OWTHEME_ROLE_TITLE_FG,
    OWTHEME_ROLE_BUTTON_BG,
    OWTHEME_ROLE_BUTTON_FG,
    OWTHEME_ROLE_ACCENT,
    OWTHEME_ROLE_BORDER,
    OWTHEME_ROLE_MENU_BG,
    OWTHEME_ROLE_MENU_FG,
    OWTHEME_ROLE_TOOLTIP_BG,
    OWTHEME_ROLE_TOOLTIP_FG,
    OWTHEME_ROLE_COUNT
} owtheme_role_t;

/* Fully materialized theme. Colors are 0x00RRGGBB; roles at or beyond
 * OWTHEME_ROLE_COUNT are reserved and ignored. */
typedef struct {
    char     name[OWTHEME_NAME_MAX];
    char     font[OWTHEME_NAME_MAX];
    uint32_t font_size;
    uint32_t flags;
    uint32_t colors[OWTHEME_MAX_ROLES];
} owtheme_descriptor_t;

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize the engine with the built-in default (dark) theme. */
bool owtheme_init(void);

/* Stage a theme descriptor; call owtheme_apply() to commit it. */
bool owtheme_load(const owtheme_descriptor_t *desc);

/* Commit the staged theme. Returns false when there is nothing staged. */
bool owtheme_apply(void);

/* Active color for a role, 0 when the role is out of range. */
uint32_t owtheme_get_color(owtheme_role_t role);

/* Name of the active font face (never NULL; empty when unset). */
const char *owtheme_get_font(void);

/* Active font pixel size. */
uint32_t owtheme_get_font_size(void);

/* Monotonic counter bumped by each successful apply. */
uint32_t owtheme_get_revision(void);

#endif /* OWTHEME64_H */
