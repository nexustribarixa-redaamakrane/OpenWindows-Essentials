/*
 * kbdlayout.h - OpenWindows Keyboard Layout Manager (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Translates scancode set 1 make codes into characters using built-in
 * layout tables (US, DE, FR, UK). Shift, caps lock and control state are
 * applied on top of the selected layout.
 */

#ifndef KBDLAYOUT_H
#define KBDLAYOUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define KBL_SCANCODE_MAX        128u    /* set 1 make codes are 0..0x7F  */
#define KBL_SCANCODE_BREAK      0x80u   /* bit 7 marks a break code      */
#define KBL_SCANCODE_EXTENDED   0xE0u   /* extended (0xE0) prefix        */
#define KBL_NUM_LAYOUTS         4u

/* Scancode set 1 key codes used for modifier tracking. */
#define KBL_SCAN_LCTRL          0x1Du
#define KBL_SCAN_LSHIFT         0x2Au
#define KBL_SCAN_RSHIFT         0x36u
#define KBL_SCAN_LALT           0x38u
#define KBL_SCAN_CAPSLOCK       0x3Au

/* Latin-1 characters for keys that have no ASCII representation. */
#define KBL_CH_NONE             0x00u
#define KBL_CH_GRAVE            0x60u   /* `                             */
#define KBL_CH_SECTION          0xA7u   /* section sign                  */
#define KBL_CH_DEGREE           0xB0u   /* degree sign                   */
#define KBL_CH_ACUTE            0xB4u   /* acute accent                  */
#define KBL_CH_MICRO            0xB5u   /* micro sign                    */
#define KBL_CH_CEDILLA          0xE7u   /* c-cedilla                     */
#define KBL_CH_AGRAVE           0xE0u   /* a-grave                       */
#define KBL_CH_EGRAVE           0xE8u   /* e-grave                       */
#define KBL_CH_EACUTE           0xE9u   /* e-acute                       */
#define KBL_CH_UGRAVE           0xF9u   /* u-grave                       */
#define KBL_CH_AE               0xE4u   /* a-umlaut                      */
#define KBL_CH_OE               0xF6u   /* o-umlaut                      */
#define KBL_CH_UE               0xFCu   /* u-umlaut                      */
#define KBL_CH_SS               0xDFu   /* sharp s                       */
#define KBL_CH_CURRENCY         0xA4u   /* currency sign                 */
#define KBL_CH_POUND            0xA3u   /* pound sign                    */
#define KBL_CH_NOT              0xACu   /* logical not sign              */

/* Supported layouts. */
typedef enum {
    KBL_LAYOUT_US = 0,
    KBL_LAYOUT_DE = 1,
    KBL_LAYOUT_FR = 2,
    KBL_LAYOUT_UK = 3
} kbl_layout_t;

/* Live modifier state. */
typedef struct {
    bool shift;
    bool ctrl;
    bool alt;
    bool caps;
} kbl_modifiers_t;

/* Layout manager state, owned by the caller. */
typedef struct {
    kbl_layout_t    layout;     /* active layout                        */
    kbl_modifiers_t modifiers;  /* active modifier state                */
    bool            initialized;
} kbl_state_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Initialise the manager with the US layout and cleared modifiers. */
bool kbl_init(kbl_state_t *state);

/* Select a built-in layout. */
bool kbl_set_layout(kbl_state_t *state, kbl_layout_t layout);

/* Translate a set 1 scancode using the state's layout and modifiers.
 * Returns '\0' for break codes and keys with no character mapping. */
char kbl_scancode_to_char(const kbl_state_t *state, uint8_t scancode);

/* Report the active layout. */
kbl_layout_t kbl_get_current(const kbl_state_t *state);

/* Human readable layout name (never NULL). */
const char *kbl_layout_name(kbl_layout_t layout);

/* Update modifier state from a scancode press/release. */
void kbl_update_modifiers(kbl_state_t *state, uint8_t scancode, bool pressed);

/* True when scancode carries the break (release) bit. */
bool kbl_is_break_code(uint8_t scancode);

#endif /* KBDLAYOUT_H */
