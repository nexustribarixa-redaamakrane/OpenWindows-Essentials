/*
 * i8042.c - OpenWindows i8042 PS/2 Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * The i8042 communicates through two ports: 0x60 carries data and 0x64
 * carries the status register (read) or a controller command (write).
 * Every transfer is gated on the input/output buffer flags so the driver
 * never races the controller.
 */

#include "i8042.h"

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

/* A write to port 0x80 costs roughly one ISA bus cycle and is the
 * traditional way to give the controller time to settle. */
static void i8042_pulse(void)
{
    ow_outb(I8042_PULSE_PORT, 0u);
}

/* ── Status / buffer handshakes ───────────────────────────────── */

uint8_t i8042_read_status(void)
{
    return ow_inb(I8042_STATUS_PORT);
}

bool i8042_wait_input_clear(void)
{
    uint32_t budget = I8042_TIMEOUT;

    while (budget != 0u) {
        if ((ow_inb(I8042_STATUS_PORT) & I8042_STATUS_IBF) == 0u) {
            return true;
        }
        budget--;
    }
    return false;
}

bool i8042_wait_output_full(void)
{
    uint32_t budget = I8042_TIMEOUT;

    while (budget != 0u) {
        if ((ow_inb(I8042_STATUS_PORT) & I8042_STATUS_OBF) != 0u) {
            return true;
        }
        budget--;
    }
    return false;
}

/* ── Transfers ────────────────────────────────────────────────── */

bool i8042_send_cmd(uint8_t command)
{
    if (!i8042_wait_input_clear()) {
        return false;
    }
    ow_outb(I8042_CMD_PORT, command);
    i8042_pulse();
    return true;
}

bool i8042_send_data(uint8_t value)
{
    if (!i8042_wait_input_clear()) {
        return false;
    }
    ow_outb(I8042_DATA_PORT, value);
    i8042_pulse();
    return true;
}

bool i8042_read_data(uint8_t *out)
{
    if (out == NULL) {
        return false;
    }
    if (!i8042_wait_output_full()) {
        return false;
    }
    *out = ow_inb(I8042_DATA_PORT);
    return true;
}

void i8042_flush(void)
{
    uint32_t budget = 64u;      /* bounded: at most 64 buffered bytes */

    /* Discard everything currently waiting in the output buffer. The
     * loop is bounded so a stuck controller cannot hang the driver.
     * Also discard the acknowledgement byte produced by a command. */
    while (budget != 0u) {
        if ((ow_inb(I8042_STATUS_PORT) & I8042_STATUS_OBF) == 0u) {
            break;
        }
        (void)ow_inb(I8042_DATA_PORT);
        i8042_pulse();
        budget--;
    }
}

/* ── Controller configuration ─────────────────────────────────── */

bool i8042_read_config(uint8_t *config)
{
    uint8_t value;

    if (config == NULL) {
        return false;
    }
    if (!i8042_send_cmd(I8042_CMD_READ_CONFIG)) {
        return false;
    }
    if (!i8042_read_data(&value)) {
        return false;
    }
    *config = value;
    return true;
}

bool i8042_write_config(uint8_t config)
{
    if (!i8042_send_cmd(I8042_CMD_WRITE_CONFIG)) {
        return false;
    }
    return i8042_send_data(config);
}

bool i8042_self_test(uint8_t *result)
{
    uint8_t value = 0u;

    if (!i8042_send_cmd(I8042_CMD_SELF_TEST)) {
        return false;
    }
    if (!i8042_read_data(&value)) {
        return false;
    }
    if (result != NULL) {
        *result = value;
    }
    return value == I8042_SELF_TEST_OK;
}

/* ── Initialisation ───────────────────────────────────────────── */

bool i8042_init(i8042_state_t *state)
{
    uint8_t config = 0u;
    uint8_t self_test = 0u;

    if (state == NULL) {
        return false;
    }

    state->config = 0u;
    state->self_test = 0u;
    state->kbd_test = 0u;
    state->aux_test = 0u;
    state->initialized = false;

    /* 1. Disable both device ports and drop whatever they queued. */
    (void)i8042_send_cmd(I8042_CMD_DISABLE_KBD);
    (void)i8042_send_cmd(I8042_CMD_DISABLE_AUX);
    i8042_flush();

    /* 2. Read the configuration byte and enable clocks, translation and
     *    interrupts (clock-disable bits are active low). */
    if (i8042_read_config(&config)) {
        state->config = config;
    }
    config |= I8042_CFG_KBD_INT | I8042_CFG_AUX_INT | I8042_CFG_KBD_XLAT;
    config &= (uint8_t)~(I8042_CFG_KBD_CLK | I8042_CFG_AUX_CLK);
    if (!i8042_write_config(config)) {
        return false;
    }
    state->config = config;

    /* 3. Controller self test (0xAA must answer with 0x55). */
    if (!i8042_self_test(&self_test)) {
        state->self_test = self_test;
        return false;
    }
    state->self_test = self_test;

    /* 4. Optionally verify both device ports report "no error" (0x00).
     *    Failing ports do not abort init; the state records the result. */
    if (i8042_send_cmd(I8042_CMD_TEST_KBD)) {
        uint8_t kbd = 0xFFu;
        (void)i8042_read_data(&kbd);
        state->kbd_test = kbd;
    }
    if (i8042_send_cmd(I8042_CMD_TEST_AUX)) {
        uint8_t aux = 0xFFu;
        (void)i8042_read_data(&aux);
        state->aux_test = aux;
    }

    /* 5. Re-enable both device ports. */
    if (!i8042_send_cmd(I8042_CMD_ENABLE_AUX)) {
        return false;
    }
    if (!i8042_send_cmd(I8042_CMD_ENABLE_KBD)) {
        return false;
    }

    state->initialized = true;
    return true;
}
