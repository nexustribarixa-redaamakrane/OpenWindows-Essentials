/*
 * kbdlayout.c - OpenWindows Keyboard Layout Manager (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Translates scancode set 1 make codes into characters using built-in
 * layout tables (US, DE, FR, UK). Shift, caps lock and control state are
 * applied on top of the selected layout.
 */

#include "kbdlayout.h"

/* ── Character classification ─────────────────────────────────── */

/* True when c is an upper case ASCII letter. */
static bool kbl_is_upper(char c)
{
    return (c >= 'A') && (c <= 'Z');
}

/* True when c is a lower case ASCII letter. */
static bool kbl_is_lower(char c)
{
    return (c >= 'a') && (c <= 'z');
}

/* ── Layout tables ────────────────────────────────────────────── */

/* US base table. Scancode set 1 make codes without modifiers. */
static const uint8_t kbl_us_base[KBL_SCANCODE_MAX] = {
    [0x01] = 0x1Bu,                                              /* Esc   */
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = 0x08u,                                              /* Back  */
    [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\r',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = KBL_CH_GRAVE,
    [0x2B] = '\\',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x37] = '*',                                                /* Kp *  */
    [0x39] = ' '
};

/* US table with shift held. */
static const uint8_t kbl_us_shift[KBL_SCANCODE_MAX] = {
    [0x01] = 0x1Bu,
    [0x02] = '!', [0x03] = '@', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x0E] = 0x08u,
    [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\r',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '"', [0x29] = '~',
    [0x2B] = '|',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M',
    [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x37] = '*',
    [0x39] = ' '
};

/* German (QWERTZ) base table. */
static const uint8_t kbl_de_base[KBL_SCANCODE_MAX] = {
    [0x01] = 0x1Bu,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = KBL_CH_SS, [0x0D] = KBL_CH_ACUTE,
    [0x0E] = 0x08u,
    [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'z', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = KBL_CH_UE, [0x1B] = '+',
    [0x1C] = '\r',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = KBL_CH_OE, [0x28] = KBL_CH_AE, [0x29] = KBL_CH_DEGREE,
    [0x2B] = '#',
    [0x2C] = 'y', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '-',
    [0x37] = '*',
    [0x39] = ' '
};

/* German table with shift held. */
static const uint8_t kbl_de_shift[KBL_SCANCODE_MAX] = {
    [0x01] = 0x1Bu,
    [0x02] = '!', [0x03] = '"', [0x04] = KBL_CH_SECTION, [0x05] = '$',
    [0x06] = '%', [0x07] = '&', [0x08] = '/', [0x09] = '(',
    [0x0A] = ')', [0x0B] = '=', [0x0C] = '?', [0x0D] = KBL_CH_GRAVE,
    [0x0E] = 0x08u,
    [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Z', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = KBL_CH_UE, [0x1B] = '*',
    [0x1C] = '\r',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = KBL_CH_OE, [0x28] = KBL_CH_AE, [0x29] = KBL_CH_DEGREE,
    [0x2B] = '\'',
    [0x2C] = 'Y', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M',
    [0x33] = ';', [0x34] = ':', [0x35] = '_',
    [0x37] = '*',
    [0x39] = ' '
};

/* French (AZERTY) base table. */
static const uint8_t kbl_fr_base[KBL_SCANCODE_MAX] = {
    [0x01] = 0x1Bu,
    [0x02] = '&', [0x03] = KBL_CH_EACUTE, [0x04] = '"', [0x05] = '\'',
    [0x06] = '(', [0x07] = '-', [0x08] = KBL_CH_EGRAVE, [0x09] = '_',
    [0x0A] = KBL_CH_CEDILLA, [0x0B] = KBL_CH_AGRAVE, [0x0C] = ')', [0x0D] = '=',
    [0x0E] = 0x08u,
    [0x0F] = '\t',
    [0x10] = 'a', [0x11] = 'z', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '^', [0x1B] = '$',
    [0x1C] = '\r',
    [0x1E] = 'q', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = 'm', [0x28] = KBL_CH_MICRO, [0x29] = KBL_CH_NOT,
    [0x2B] = '*',
    [0x2C] = 'w', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = ',',
    [0x33] = ';', [0x34] = ':', [0x35] = '!',
    [0x37] = '*',
    [0x39] = ' '
};

/* French table with shift held. */
static const uint8_t kbl_fr_shift[KBL_SCANCODE_MAX] = {
    [0x01] = 0x1Bu,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = KBL_CH_DEGREE, [0x0D] = '+',
    [0x0E] = 0x08u,
    [0x0F] = '\t',
    [0x10] = 'A', [0x11] = 'Z', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = KBL_CH_UE, [0x1B] = KBL_CH_POUND,
    [0x1C] = '\r',
    [0x1E] = 'Q', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = 'M', [0x28] = '%', [0x29] = KBL_CH_NOT,
    [0x2B] = KBL_CH_MICRO,
    [0x2C] = 'W', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = '?',
    [0x33] = '.', [0x34] = '/', [0x35] = KBL_CH_SECTION,
    [0x37] = '*',
    [0x39] = ' '
};

/* UK base table (US layout with a few substitutions). */
static const uint8_t kbl_uk_base[KBL_SCANCODE_MAX] = {
    [0x01] = 0x1Bu,
    [0x02] = '1', [0x03] = '2', [0x04] = '3', [0x05] = '4',
    [0x06] = '5', [0x07] = '6', [0x08] = '7', [0x09] = '8',
    [0x0A] = '9', [0x0B] = '0', [0x0C] = '-', [0x0D] = '=',
    [0x0E] = 0x08u,
    [0x0F] = '\t',
    [0x10] = 'q', [0x11] = 'w', [0x12] = 'e', [0x13] = 'r',
    [0x14] = 't', [0x15] = 'y', [0x16] = 'u', [0x17] = 'i',
    [0x18] = 'o', [0x19] = 'p', [0x1A] = '[', [0x1B] = ']',
    [0x1C] = '\r',
    [0x1E] = 'a', [0x1F] = 's', [0x20] = 'd', [0x21] = 'f',
    [0x22] = 'g', [0x23] = 'h', [0x24] = 'j', [0x25] = 'k',
    [0x26] = 'l', [0x27] = ';', [0x28] = '\'', [0x29] = KBL_CH_GRAVE,
    [0x2B] = '#',
    [0x2C] = 'z', [0x2D] = 'x', [0x2E] = 'c', [0x2F] = 'v',
    [0x30] = 'b', [0x31] = 'n', [0x32] = 'm',
    [0x33] = ',', [0x34] = '.', [0x35] = '/',
    [0x37] = '*',
    [0x39] = ' '
};

/* UK table with shift held. */
static const uint8_t kbl_uk_shift[KBL_SCANCODE_MAX] = {
    [0x01] = 0x1Bu,
    [0x02] = '!', [0x03] = '"', [0x04] = '#', [0x05] = '$',
    [0x06] = '%', [0x07] = '^', [0x08] = '&', [0x09] = '*',
    [0x0A] = '(', [0x0B] = ')', [0x0C] = '_', [0x0D] = '+',
    [0x0E] = 0x08u,
    [0x0F] = '\t',
    [0x10] = 'Q', [0x11] = 'W', [0x12] = 'E', [0x13] = 'R',
    [0x14] = 'T', [0x15] = 'Y', [0x16] = 'U', [0x17] = 'I',
    [0x18] = 'O', [0x19] = 'P', [0x1A] = '{', [0x1B] = '}',
    [0x1C] = '\r',
    [0x1E] = 'A', [0x1F] = 'S', [0x20] = 'D', [0x21] = 'F',
    [0x22] = 'G', [0x23] = 'H', [0x24] = 'J', [0x25] = 'K',
    [0x26] = 'L', [0x27] = ':', [0x28] = '@', [0x29] = KBL_CH_NOT,
    [0x2B] = '~',
    [0x2C] = 'Z', [0x2D] = 'X', [0x2E] = 'C', [0x2F] = 'V',
    [0x30] = 'B', [0x31] = 'N', [0x32] = 'M',
    [0x33] = '<', [0x34] = '>', [0x35] = '?',
    [0x37] = '*',
    [0x39] = ' '
};

/* Human readable layout names, indexed by kbl_layout_t. */
static const char *const kbl_layout_names[KBL_NUM_LAYOUTS] = {
    "US",
    "DE",
    "FR",
    "UK"
};

/* ── Table selection ──────────────────────────────────────────── */

/* Return the unshifted table for a layout, or NULL when out of range. */
static const uint8_t *kbl_base_table(kbl_layout_t layout)
{
    switch (layout) {
    case KBL_LAYOUT_US: return kbl_us_base;
    case KBL_LAYOUT_DE: return kbl_de_base;
    case KBL_LAYOUT_FR: return kbl_fr_base;
    case KBL_LAYOUT_UK: return kbl_uk_base;
    default:            return NULL;
    }
}

/* Return the shifted table for a layout, or NULL when out of range. */
static const uint8_t *kbl_shift_table(kbl_layout_t layout)
{
    switch (layout) {
    case KBL_LAYOUT_US: return kbl_us_shift;
    case KBL_LAYOUT_DE: return kbl_de_shift;
    case KBL_LAYOUT_FR: return kbl_fr_shift;
    case KBL_LAYOUT_UK: return kbl_uk_shift;
    default:            return NULL;
    }
}

/* ── Modifier state ───────────────────────────────────────────── */

bool kbl_is_break_code(uint8_t scancode)
{
    return (scancode & KBL_SCANCODE_BREAK) != 0u;
}

void kbl_update_modifiers(kbl_state_t *state, uint8_t scancode, bool pressed)
{
    uint8_t code;

    if (state == NULL) {
        return;
    }

    /* A scancode carrying bit 7 is always a release, regardless of the
     * caller supplied flag. */
    if (kbl_is_break_code(scancode)) {
        pressed = false;
    }

    code = (uint8_t)(scancode & (uint8_t)~KBL_SCANCODE_BREAK);

    switch (code) {
    case KBL_SCAN_LSHIFT:
    case KBL_SCAN_RSHIFT:
        state->modifiers.shift = pressed;
        break;
    case KBL_SCAN_LCTRL:
        state->modifiers.ctrl = pressed;
        break;
    case KBL_SCAN_LALT:
        state->modifiers.alt = pressed;
        break;
    case KBL_SCAN_CAPSLOCK:
        if (pressed) {
            state->modifiers.caps = !state->modifiers.caps;
        }
        break;
    default:
        break;
    }
}

/* ── Public API ───────────────────────────────────────────────── */

bool kbl_init(kbl_state_t *state)
{
    if (state == NULL) {
        return false;
    }

    state->layout = KBL_LAYOUT_US;
    state->modifiers.shift = false;
    state->modifiers.ctrl = false;
    state->modifiers.alt = false;
    state->modifiers.caps = false;
    state->initialized = true;
    return true;
}

bool kbl_set_layout(kbl_state_t *state, kbl_layout_t layout)
{
    if ((state == NULL) || !state->initialized) {
        return false;
    }
    if ((uint32_t)layout >= KBL_NUM_LAYOUTS) {
        return false;
    }

    state->layout = layout;
    return true;
}

kbl_layout_t kbl_get_current(const kbl_state_t *state)
{
    if ((state == NULL) || !state->initialized) {
        return KBL_LAYOUT_US;
    }
    return state->layout;
}

const char *kbl_layout_name(kbl_layout_t layout)
{
    if ((uint32_t)layout >= KBL_NUM_LAYOUTS) {
        return kbl_layout_names[KBL_LAYOUT_US];
    }
    return kbl_layout_names[(uint32_t)layout];
}

char kbl_scancode_to_char(const kbl_state_t *state, uint8_t scancode)
{
    const uint8_t *table;
    uint8_t raw;
    char c;

    if ((state == NULL) || !state->initialized) {
        return '\0';
    }

    /* Break codes, the 0xE0 extended prefix and unmapped keys yield no
     * character at this level. */
    if ((scancode & KBL_SCANCODE_BREAK) != 0u) {
        return '\0';
    }
    if ((uint32_t)scancode >= KBL_SCANCODE_MAX) {
        return '\0';
    }

    table = state->modifiers.shift ? kbl_shift_table(state->layout)
                                   : kbl_base_table(state->layout);
    if (table == NULL) {
        return '\0';
    }

    raw = table[scancode];
    if (raw == KBL_CH_NONE) {
        return '\0';
    }

    c = (char)raw;

    /* Caps lock only flips the case of ASCII letters, effectively acting
     * as an XOR against the shift state. */
    if (state->modifiers.caps) {
        if (kbl_is_lower(c)) {
            c = (char)(c - 'a' + 'A');
        } else if (kbl_is_upper(c)) {
            c = (char)(c - 'A' + 'a');
        }
    }

    /* Control collapses ASCII letters to their 0x01..0x1A codes. */
    if (state->modifiers.ctrl) {
        if (kbl_is_lower(c)) {
            c = (char)((uint8_t)c & 0x1Fu);
        } else if (kbl_is_upper(c)) {
            c = (char)((uint8_t)c & 0x1Fu);
        }
    }

    return c;
}
