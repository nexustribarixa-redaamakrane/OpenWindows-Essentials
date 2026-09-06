/*
 * ps2hid.c - OpenWindows PS/2 Keyboard & Mouse Input Driver Implementation (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "ps2hid.h"

void ps2_init(ps2_device_state_t *state)
{
    if (!state) return;
    state->key_head = 0u;
    state->key_tail = 0u;
    state->key_count = 0u;
    state->mouse_head = 0u;
    state->mouse_tail = 0u;
    state->mouse_count = 0u;
    state->shift_pressed = false;
    state->ctrl_pressed = false;
    state->alt_pressed = false;
}

static uint32_t scancode_to_sucs(uint8_t sc, bool shift)
{
    /* Basic Set 1 translation */
    if (sc == 0x1C) return 0x000A; /* Enter */
    if (sc == 0x0E) return 0x0008; /* Backspace */
    if (sc == 0x0F) return 0x0009; /* Tab */
    if (sc == 0x39) return 0x0020; /* Space */

    if (sc >= 0x10 && sc <= 0x19) {
        static const char qwer[] = "qwertyuiop";
        static const char QWER[] = "QWERTYUIOP";
        return shift ? QWER[sc - 0x10] : qwer[sc - 0x10];
    }
    if (sc >= 0x1E && sc <= 0x26) {
        static const char asdf[] = "asdfghjkl";
        static const char ASDF[] = "ASDFGHJKL";
        return shift ? ASDF[sc - 0x1E] : asdf[sc - 0x1E];
    }
    if (sc >= 0x2C && sc <= 0x32) {
        static const char zxcv[] = "zxcvbnm";
        static const char ZXCV[] = "ZXCVBNM";
        return shift ? ZXCV[sc - 0x2C] : zxcv[sc - 0x2C];
    }
    if (sc >= 0x02 && sc <= 0x0B) {
        static const char num[] = "1234567890";
        static const char sym[] = "!@#$%^&*()";
        return shift ? sym[sc - 0x02] : num[sc - 0x02];
    }
    return 0;
}

bool ps2_poll_keyboard(ps2_device_state_t *state, uint8_t raw_byte, ps2_key_event_t *out)
{
    if (!state || !out) return false;

    bool is_released = (raw_byte & 0x80u) != 0;
    uint8_t sc = raw_byte & 0x7Fu;

    if (sc == 0x2A || sc == 0x36) {
        state->shift_pressed = !is_released;
    } else if (sc == 0x1D) {
        state->ctrl_pressed = !is_released;
    } else if (sc == 0x38) {
        state->alt_pressed = !is_released;
    }

    out->scancode = sc;
    out->is_released = is_released;
    out->shift_held = state->shift_pressed;
    out->sucs_codepoint = scancode_to_sucs(sc, state->shift_pressed);

    if (state->key_count < PS2_RING_BUFFER_SIZE) {
        state->key_ring[state->key_head] = *out;
        state->key_head = (state->key_head + 1u) % PS2_RING_BUFFER_SIZE;
        state->key_count++;
    }

    return true;
}

bool ps2_poll_mouse(ps2_device_state_t *state, const uint8_t packet[3], ps2_mouse_event_t *out)
{
    if (!state || !packet || !out) return false;

    out->btn_left = (packet[0] & 0x01u) != 0;
    out->btn_right = (packet[0] & 0x02u) != 0;
    out->btn_middle = (packet[0] & 0x04u) != 0;

    int16_t dx = (int16_t)packet[1];
    int16_t dy = (int16_t)packet[2];
    if (packet[0] & 0x10u) dx |= (int16_t)0xFF00;
    if (packet[0] & 0x20u) dy |= (int16_t)0xFF00;

    out->delta_x = dx;
    out->delta_y = -dy; /* Invert Y coordinate */

    if (state->mouse_count < PS2_RING_BUFFER_SIZE) {
        state->mouse_ring[state->mouse_head] = *out;
        state->mouse_head = (state->mouse_head + 1u) % PS2_RING_BUFFER_SIZE;
        state->mouse_count++;
    }

    return true;
}
