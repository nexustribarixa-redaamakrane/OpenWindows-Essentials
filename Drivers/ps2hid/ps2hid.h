/*
 * ps2hid.h - OpenWindows PS/2 Keyboard & Mouse Input Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef PS2HID_H
#define PS2HID_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define PS2_DATA_PORT       0x60
#define PS2_STATUS_PORT     0x64
#define PS2_COMMAND_PORT    0x64

#define PS2_RING_BUFFER_SIZE 64u

typedef struct {
    uint8_t  scancode;
    uint32_t sucs_codepoint;
    bool     is_released;
    bool     shift_held;
} ps2_key_event_t;

typedef struct {
    int16_t  delta_x;
    int16_t  delta_y;
    bool     btn_left;
    bool     btn_right;
    bool     btn_middle;
} ps2_mouse_event_t;

typedef struct {
    ps2_key_event_t key_ring[PS2_RING_BUFFER_SIZE];
    uint16_t        key_head;
    uint16_t        key_tail;
    uint16_t        key_count;

    ps2_mouse_event_t mouse_ring[PS2_RING_BUFFER_SIZE];
    uint16_t        mouse_head;
    uint16_t        mouse_tail;
    uint16_t        mouse_count;

    bool            shift_pressed;
    bool            ctrl_pressed;
    bool            alt_pressed;
} ps2_device_state_t;

void ps2_init(ps2_device_state_t *state);
bool ps2_poll_keyboard(ps2_device_state_t *state, uint8_t raw_byte, ps2_key_event_t *out);
bool ps2_poll_mouse(ps2_device_state_t *state, const uint8_t packet[3], ps2_mouse_event_t *out);

#endif /* PS2HID_H */
