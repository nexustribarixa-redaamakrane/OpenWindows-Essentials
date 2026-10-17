/*
 * mousedrv.c - OpenWindows Mouse Input Driver (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Talks to the PS/2 auxiliary device through the i8042 controller. Every
 * device byte is written by first issuing the 0xD4 "write to auxiliary"
 * command and then placing the byte in port 0x60. Replies are read back
 * from port 0x60 only when the status register marks them as auxiliary
 * data, so keyboard traffic is never mistaken for mouse input.
 */

#include "mousedrv.h"

/* ── Port I/O helpers ─────────────────────────────────────────── */

static inline uint8_t ow_inb(uint16_t port)
{
    uint8_t v;
    __asm__ __volatile__("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline void ow_outb(uint16_t port, uint8_t value)
{
    __asm__ __volatile__("outb %0, %1" : : "a"(value), "Nd"(port));
}

/* ── i8042 helpers ────────────────────────────────────────────── */

/* One ISA cycle of settling time. */
static void mouse_pulse(void)
{
    ow_outb(MOUSE_PULSE_PORT, 0u);
}

/* Wait until the controller input buffer is empty. */
static bool mouse_wait_input_clear(void)
{
    uint32_t budget = MOUSE_TIMEOUT;

    while (budget != 0u) {
        if ((ow_inb(MOUSE_STATUS_PORT) & MOUSE_STATUS_IBF) == 0u) {
            return true;
        }
        budget--;
    }
    return false;
}

/* Wait until a byte is available in the output buffer. */
static bool mouse_wait_output_full(void)
{
    uint32_t budget = MOUSE_TIMEOUT;

    while (budget != 0u) {
        if ((ow_inb(MOUSE_STATUS_PORT) & MOUSE_STATUS_OBF) != 0u) {
            return true;
        }
        budget--;
    }
    return false;
}

/* Send a controller command byte to port 0x64. */
static bool mouse_ctrl_cmd(uint8_t command)
{
    if (!mouse_wait_input_clear()) {
        return false;
    }
    ow_outb(MOUSE_CMD_PORT, command);
    mouse_pulse();
    return true;
}

/* Send a data byte to port 0x60 (already addressed by a command). */
static bool mouse_ctrl_data(uint8_t value)
{
    if (!mouse_wait_input_clear()) {
        return false;
    }
    ow_outb(MOUSE_DATA_PORT, value);
    mouse_pulse();
    return true;
}

/* Read any byte from port 0x60 (controller or device). */
static bool mouse_read_raw(uint8_t *out)
{
    if (!mouse_wait_output_full()) {
        return false;
    }
    *out = ow_inb(MOUSE_DATA_PORT);
    return true;
}

/* Read a byte that came specifically from the auxiliary device. Keyboard
 * bytes are dropped so the packet stream stays aligned. */
static bool mouse_read_aux(uint8_t *out)
{
    uint32_t budget = MOUSE_TIMEOUT;

    while (budget != 0u) {
        uint8_t status = ow_inb(MOUSE_STATUS_PORT);
        if ((status & MOUSE_STATUS_OBF) != 0u) {
            uint8_t value = ow_inb(MOUSE_DATA_PORT);
            if ((status & MOUSE_STATUS_AUXB) != 0u) {
                *out = value;
                return true;
            }
        }
        budget--;
    }
    return false;
}

/* Write one command byte to the auxiliary device (via the 0xD4 escape). */
static bool mouse_write_aux(uint8_t value)
{
    if (!mouse_ctrl_cmd(MOUSE_CMD_WRITE_AUX)) {
        return false;
    }
    return mouse_ctrl_data(value);
}

/* Write a device command and consume its acknowledge byte. One resend is
 * retried, as some emulated controllers lose the first byte. */
static bool mouse_command(uint8_t command)
{
    uint8_t ack = 0u;

    if (!mouse_write_aux(command)) {
        return false;
    }
    if (!mouse_read_aux(&ack)) {
        return false;
    }
    if (ack == MOUSE_RESEND) {
        if (!mouse_write_aux(command)) {
            return false;
        }
        if (!mouse_read_aux(&ack)) {
            return false;
        }
    }
    return ack == MOUSE_ACK;
}

/* Decode a validated three byte packet into the accumulated state. */
static void mouse_decode(mouse_state_t *state, const mouse_packet_t *packet)
{
    uint8_t flags = packet->bytes[0];
    int32_t dx = (int32_t)packet->bytes[1];
    int32_t dy = (int32_t)packet->bytes[2];

    /* Bytes 1 and 2 are magnitudes; the sign lives in the flag byte. */
    if ((flags & 0x10u) != 0u) {
        dx -= 256;
    }
    if ((flags & 0x20u) != 0u) {
        dy -= 256;
    }
    /* PS/2 Y grows upwards; flip it for screen coordinates. */
    dy = -dy;

    state->last_dx = dx;
    state->last_dy = dy;
    state->x += dx;
    state->y += dy;
    state->buttons = (uint8_t)(flags & 0x07u);
}

/* ── Public API ───────────────────────────────────────────────── */

bool mouse_read_packet(mouse_state_t *state, mouse_packet_t *packet)
{
    uint32_t guard = MOUSE_PACKET_SIZE * 16u;   /* bounded resync window */

    if (state == NULL) {
        return false;
    }
    state->packet.index = 0u;

    while (guard != 0u) {
        uint8_t byte = 0u;

        guard--;
        if (!mouse_read_aux(&byte)) {
            state->packet.index = 0u;
            return false;
        }

        /* Byte 0 must carry the sync bit; otherwise drop it and resync. */
        if ((state->packet.index == 0u) &&
            ((byte & MOUSE_PACKET_SYNC) == 0u)) {
            continue;
        }

        state->packet.bytes[state->packet.index] = byte;
        state->packet.index++;

        if (state->packet.index >= MOUSE_PACKET_SIZE) {
            if (packet != NULL) {
                *packet = state->packet;
            }
            state->packet.index = 0u;
            return true;
        }
    }

    state->packet.index = 0u;
    return false;
}

bool mouse_poll(mouse_state_t *state)
{
    uint32_t budget = 32u;      /* do not spin forever on a noisy port */
    bool progress = false;

    if ((state == NULL) || !state->initialized) {
        return false;
    }

    while (budget != 0u) {
        mouse_packet_t packet;

        if ((ow_inb(MOUSE_STATUS_PORT) & MOUSE_STATUS_OBF) == 0u) {
            break;
        }
        if (!mouse_read_packet(state, &packet)) {
            break;
        }
        mouse_decode(state, &packet);
        progress = true;
        budget--;
    }
    return progress;
}

bool mouse_get_state(const mouse_state_t *state, mouse_report_t *report)
{
    if ((state == NULL) || (report == NULL)) {
        return false;
    }
    report->x = state->x;
    report->y = state->y;
    report->dx = state->last_dx;
    report->dy = state->last_dy;
    report->buttons = state->buttons;
    report->left = (state->buttons & 0x01u) != 0u;
    report->right = (state->buttons & 0x02u) != 0u;
    report->middle = (state->buttons & 0x04u) != 0u;
    return true;
}

bool mouse_set_rate(mouse_state_t *state, uint8_t rate)
{
    if (state == NULL) {
        return false;
    }
    switch (rate) {
    case 10u:
    case 20u:
    case 40u:
    case 60u:
    case 80u:
    case 100u:
    case 200u:
        break;
    default:
        return false;
    }

    /* The device expects 0xF3 followed by the requested rate, each
     * acknowledged with 0xFA. */
    if (!mouse_command(MOUSE_DEV_SET_RATE)) {
        return false;
    }
    if (!mouse_command(rate)) {
        return false;
    }
    state->sample_rate = rate;
    return true;
}

bool mouse_init(mouse_state_t *state)
{
    uint8_t config = 0u;
    uint8_t reply = 0u;

    if (state == NULL) {
        return false;
    }

    state->x = 0;
    state->y = 0;
    state->last_dx = 0;
    state->last_dy = 0;
    state->buttons = 0u;
    state->device_id = 0u;
    state->sample_rate = MOUSE_DEFAULT_RATE;
    state->resolution = MOUSE_DEFAULT_RES;
    state->packet.index = 0u;
    state->initialized = false;

    /* 1. Enable the auxiliary port and its clock in the controller. */
    if (!mouse_ctrl_cmd(MOUSE_CMD_ENABLE_AUX)) {
        return false;
    }
    if (!mouse_ctrl_cmd(MOUSE_CMD_READ_CONFIG)) {
        return false;
    }
    if (!mouse_read_raw(&config)) {
        return false;
    }
    config |= MOUSE_CFG_AUX_INT;
    config &= (uint8_t)~MOUSE_CFG_AUX_CLK;
    if (!mouse_ctrl_cmd(MOUSE_CMD_WRITE_CONFIG)) {
        return false;
    }
    if (!mouse_ctrl_data(config)) {
        return false;
    }

    /* 2. Reset the device: acknowledge, self test pass, device id. */
    if (!mouse_write_aux(MOUSE_DEV_RESET)) {
        return false;
    }
    if (!mouse_read_aux(&reply) || (reply != MOUSE_ACK)) {
        return false;
    }
    if (!mouse_read_aux(&reply) || (reply != MOUSE_SELF_TEST_OK)) {
        return false;
    }
    if (mouse_read_aux(&reply)) {
        state->device_id = reply;
    }

    /* 3. Defaults, sample rate and resolution. */
    if (!mouse_command(MOUSE_DEV_SET_DEFAULTS)) {
        return false;
    }
    if (!mouse_set_rate(state, MOUSE_DEFAULT_RATE)) {
        return false;
    }
    if (!mouse_write_aux(MOUSE_DEV_SET_RES)) {
        return false;
    }
    if (!mouse_read_aux(&reply) || (reply != MOUSE_ACK)) {
        return false;
    }
    if (!mouse_write_aux(MOUSE_DEFAULT_RES)) {
        return false;
    }
    if (!mouse_read_aux(&reply) || (reply != MOUSE_ACK)) {
        return false;
    }
    state->resolution = MOUSE_DEFAULT_RES;

    /* 4. 1:1 scaling and start streaming packets. */
    if (!mouse_command(MOUSE_DEV_SET_SCALING)) {
        return false;
    }
    if (!mouse_command(MOUSE_DEV_ENABLE)) {
        return false;
    }

    state->initialized = true;
    return true;
}
