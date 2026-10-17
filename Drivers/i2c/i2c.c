/*
 * i2c.c - OpenWindows I2C Bus Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Drives a generic memory-mapped I2C master. Every transfer is a sequence
 * of byte level commands issued through the command register and completed
 * by polling the status register. All register accesses go through volatile
 * 32-bit pointers.
 */

#include "i2c.h"

/* ── Register access helpers ──────────────────────────────────── */

/* Read a 32-bit controller register at a byte offset. */
static uint32_t i2c_reg_read(const i2c_ctrl_t *ctrl, uint32_t off)
{
    return ctrl->regs[off / 4u];
}

/* Write a 32-bit controller register at a byte offset. */
static void i2c_reg_write(const i2c_ctrl_t *ctrl, uint32_t off, uint32_t value)
{
    ctrl->regs[off / 4u] = value;
}

/* Drop any latched status flags before starting a new transfer. */
static void i2c_clear_status(const i2c_ctrl_t *ctrl)
{
    i2c_reg_write(ctrl, I2C_REG_CMD, I2C_CMD_IACK);
}

/* Wait until the controller reports the current command finished.
 * Translates the latched error flags into a status code. */
static i2c_status_t i2c_wait(const i2c_ctrl_t *ctrl)
{
    uint32_t budget = I2C_TIMEOUT_LOOPS;

    while (budget != 0u) {
        uint32_t st = i2c_reg_read(ctrl, I2C_REG_STATUS);

        if ((st & I2C_STS_BUSY) == 0u) {
            if ((st & I2C_STS_TIMEOUT) != 0u) {
                return I2C_ERR_TIMEOUT;
            }
            if ((st & I2C_STS_ARB) != 0u) {
                return I2C_ERR_ARB;
            }
            if ((st & I2C_STS_NACK) != 0u) {
                return I2C_ERR_NACK;
            }
            return I2C_OK;
        }
        budget--;
    }

    return I2C_ERR_TIMEOUT;
}

/* True when ctrl is usable for a transfer. */
static bool i2c_ready(const i2c_ctrl_t *ctrl)
{
    return (ctrl != NULL) && ctrl->initialized && (ctrl->regs != NULL);
}

/* Address probe: START + address + STOP with no data phase. */
static i2c_status_t i2c_probe(const i2c_ctrl_t *ctrl, uint8_t dev_addr)
{
    i2c_clear_status(ctrl);
    i2c_reg_write(ctrl, I2C_REG_ADDR, (uint32_t)dev_addr << 1);
    i2c_reg_write(ctrl, I2C_REG_CMD, I2C_CMD_START | I2C_CMD_STOP | I2C_CMD_WRITE);
    return i2c_wait(ctrl);
}

/* ── Public API ───────────────────────────────────────────────── */

bool i2c_init(i2c_ctrl_t *ctrl, volatile uint32_t *regs,
              uint32_t base_clock_hz, uint32_t bus_hz)
{
    uint32_t prescale;
    uint32_t ctrl_val;

    if ((ctrl == NULL) || (regs == NULL)) {
        return false;
    }
    if ((base_clock_hz == 0u) || (bus_hz < I2C_MIN_CLOCK_HZ)) {
        return false;
    }

    ctrl->regs = regs;
    ctrl->base_clock_hz = base_clock_hz;
    ctrl->bus_hz = bus_hz;

    /* The core divides its input clock by 5 * (prescale + 1) to obtain the
     * SCL frequency, so invert that relationship. */
    prescale = base_clock_hz / (5u * bus_hz);
    if (prescale != 0u) {
        prescale--;
    }

    ctrl_val = I2C_CTRL_EN | I2C_CTRL_AA;
    if (bus_hz > I2C_FAST_THRESHOLD_HZ) {
        ctrl_val |= I2C_CTRL_FAST;
    }

    i2c_reg_write(ctrl, I2C_REG_PRESCALE, prescale);
    i2c_reg_write(ctrl, I2C_REG_CTRL, ctrl_val);

    ctrl->initialized = true;
    return true;
}

i2c_status_t i2c_write(i2c_ctrl_t *ctrl, uint8_t dev_addr,
                       const uint8_t *buf, size_t len)
{
    size_t i;

    if (!i2c_ready(ctrl) || (buf == NULL) || (len == 0u)) {
        return I2C_ERR_PARAM;
    }
    if (dev_addr >= 0x80u) {
        return I2C_ERR_PARAM;
    }

    i2c_reg_write(ctrl, I2C_REG_ADDR, (uint32_t)dev_addr << 1);

    for (i = 0u; i < len; i++) {
        uint32_t cmd = I2C_CMD_WRITE;
        i2c_status_t st;

        if (i == 0u) {
            cmd |= I2C_CMD_START;
        }
        if (i == (len - 1u)) {
            cmd |= I2C_CMD_STOP;
        }

        i2c_clear_status(ctrl);
        i2c_reg_write(ctrl, I2C_REG_DATA, buf[i]);
        i2c_reg_write(ctrl, I2C_REG_CMD, cmd);

        st = i2c_wait(ctrl);
        if (st != I2C_OK) {
            return st;
        }
    }

    return I2C_OK;
}

i2c_status_t i2c_read(i2c_ctrl_t *ctrl, uint8_t dev_addr,
                      uint8_t *buf, size_t len)
{
    size_t i;

    if (!i2c_ready(ctrl) || (buf == NULL) || (len == 0u)) {
        return I2C_ERR_PARAM;
    }
    if (dev_addr >= 0x80u) {
        return I2C_ERR_PARAM;
    }

    i2c_reg_write(ctrl, I2C_REG_ADDR, ((uint32_t)dev_addr << 1) | 1u);

    for (i = 0u; i < len; i++) {
        uint32_t cmd = I2C_CMD_READ;
        i2c_status_t st;

        if (i == 0u) {
            cmd |= I2C_CMD_START;
        }
        if (i == (len - 1u)) {
            /* NACK the final byte and release the bus. */
            cmd |= I2C_CMD_STOP;
        } else {
            cmd |= I2C_CMD_ACK;
        }

        i2c_clear_status(ctrl);
        i2c_reg_write(ctrl, I2C_REG_CMD, cmd);

        st = i2c_wait(ctrl);
        if (st != I2C_OK) {
            return st;
        }

        buf[i] = (uint8_t)(i2c_reg_read(ctrl, I2C_REG_DATA) & 0xFFu);
    }

    return I2C_OK;
}

i2c_status_t i2c_scan_bus(i2c_ctrl_t *ctrl, uint8_t *found,
                          size_t cap, size_t *count_out)
{
    size_t n = 0u;
    uint16_t addr;

    if (count_out != NULL) {
        *count_out = 0u;
    }
    if (!i2c_ready(ctrl)) {
        return I2C_ERR_PARAM;
    }

    for (addr = I2C_SCAN_FIRST; addr <= I2C_SCAN_LAST; addr++) {
        i2c_status_t st = i2c_probe(ctrl, (uint8_t)addr);

        if (st == I2C_OK) {
            if ((found != NULL) && (n < cap)) {
                found[n] = (uint8_t)addr;
            }
            n++;
        }
    }

    if (count_out != NULL) {
        *count_out = n;
    }
    return I2C_OK;
}
