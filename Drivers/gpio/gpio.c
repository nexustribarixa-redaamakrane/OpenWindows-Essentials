/*
 * gpio.c - OpenWindows GPIO Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Drives a generic memory-mapped GPIO block. Pin numbers are split into a
 * bank index and a bit within the bank; every bank is 32 bytes of 32-bit
 * registers. Only volatile register accesses are used and all buffers are
 * caller provided.
 */

#include "gpio.h"

/* ── Register access helpers ──────────────────────────────────── */

/* Address of a bank register for pin. */
static volatile uint32_t *gpio_bank_reg(const gpio_ctrl_t *ctrl,
                                        uint32_t pin, uint32_t reg_off)
{
    uint32_t bank = pin / GPIO_PINS_PER_BANK;
    uint32_t index = (bank * (GPIO_BANK_STRIDE / 4u)) + (reg_off / 4u);
    return &ctrl->regs[index];
}

/* Single bit mask for pin inside its bank. */
static uint32_t gpio_bit_mask(uint32_t pin)
{
    return 1u << (pin % GPIO_PINS_PER_BANK);
}

/* True when ctrl owns this pin. */
static bool gpio_valid(const gpio_ctrl_t *ctrl, uint32_t pin)
{
    return (ctrl != NULL) && ctrl->initialized &&
           (ctrl->regs != NULL) && (pin < ctrl->num_pins);
}

/* ── Public API ───────────────────────────────────────────────── */

bool gpio_init(gpio_ctrl_t *ctrl, volatile uint32_t *regs, uint32_t num_pins)
{
    uint32_t bank;

    if ((ctrl == NULL) || (regs == NULL)) {
        return false;
    }
    if ((num_pins == 0u) || (num_pins > GPIO_MAX_PINS)) {
        return false;
    }

    ctrl->regs = regs;
    ctrl->num_pins = num_pins;

    /* Start with every bank configured as input and outputs low. */
    for (bank = 0u; bank < (num_pins + GPIO_PINS_PER_BANK - 1u) / GPIO_PINS_PER_BANK; bank++) {
        uint32_t base = bank * (GPIO_BANK_STRIDE / 4u);
        ctrl->regs[base + (GPIO_REG_DIR / 4u)] = 0u;
        ctrl->regs[base + (GPIO_REG_OUT / 4u)] = 0u;
        ctrl->regs[base + (GPIO_REG_CLR / 4u)] = 0xFFFFFFFFu;
    }

    ctrl->initialized = true;
    return true;
}

bool gpio_set_direction(gpio_ctrl_t *ctrl, uint32_t pin, gpio_dir_t dir)
{
    volatile uint32_t *reg;
    uint32_t value;

    if (!gpio_valid(ctrl, pin)) {
        return false;
    }
    if ((dir != GPIO_DIR_IN) && (dir != GPIO_DIR_OUT)) {
        return false;
    }

    reg = gpio_bank_reg(ctrl, pin, GPIO_REG_DIR);
    value = *reg;
    if (dir == GPIO_DIR_OUT) {
        value |= gpio_bit_mask(pin);
    } else {
        value &= ~gpio_bit_mask(pin);
    }
    *reg = value;
    return true;
}

bool gpio_set_pin(gpio_ctrl_t *ctrl, uint32_t pin, bool level)
{
    volatile uint32_t *reg;

    if (!gpio_valid(ctrl, pin)) {
        return false;
    }

    /* Use the atomic set/clear registers so other pins are untouched. */
    reg = gpio_bank_reg(ctrl, pin, level ? GPIO_REG_SET : GPIO_REG_CLR);
    *reg = gpio_bit_mask(pin);
    return true;
}

bool gpio_get_pin(const gpio_ctrl_t *ctrl, uint32_t pin, bool *level_out)
{
    volatile uint32_t *reg;
    uint32_t value;

    if (!gpio_valid(ctrl, pin) || (level_out == NULL)) {
        return false;
    }

    reg = gpio_bank_reg(ctrl, pin, GPIO_REG_IN);
    value = *reg;
    *level_out = ((value & gpio_bit_mask(pin)) != 0u);
    return true;
}
