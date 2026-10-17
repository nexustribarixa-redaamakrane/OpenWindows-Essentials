/*
 * gpio.h - OpenWindows GPIO Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Register-level model of a generic memory-mapped GPIO block. Pins are
 * grouped into 32-bit banks; pin N lives in bank N/32, bit N%32. Each bank
 * owns a direction register and separate output/input/set/clear registers.
 * The MMIO base and pin count are supplied by the caller.
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Limits ───────────────────────────────────────────────────── */

#define GPIO_PINS_PER_BANK      32u     /* pins addressable per bank    */
#define GPIO_MAX_PINS           1024u   /* up to 32 banks of 32 pins     */

/* ── Register offsets (per bank, 32-bit registers) ────────────── */

#define GPIO_BANK_STRIDE        0x20u   /* bytes between two banks      */
#define GPIO_REG_DIR            0x00u   /* direction: 1 = output        */
#define GPIO_REG_OUT            0x04u   /* output latch                 */
#define GPIO_REG_IN             0x08u   /* sampled pin level            */
#define GPIO_REG_SET            0x0Cu   /* write 1 to drive high        */
#define GPIO_REG_CLR            0x10u   /* write 1 to drive low         */

/* ── Pin direction ────────────────────────────────────────────── */

typedef enum {
    GPIO_DIR_IN = 0,    /* high impedance input                      */
    GPIO_DIR_OUT = 1    /* push/pull output                          */
} gpio_dir_t;

/* ── Controller instance ──────────────────────────────────────── */

typedef struct {
    volatile uint32_t *regs;            /* MMIO register base           */
    uint32_t num_pins;                  /* implemented pins             */
    bool     initialized;               /* init completed               */
} gpio_ctrl_t;

/* ── API ──────────────────────────────────────────────────────── */

/* Map the controller at regs with num_pins implemented pins. */
bool gpio_init(gpio_ctrl_t *ctrl, volatile uint32_t *regs, uint32_t num_pins);

/* Set a pin to input or output. */
bool gpio_set_direction(gpio_ctrl_t *ctrl, uint32_t pin, gpio_dir_t dir);

/* Drive an output pin high (true) or low (false). */
bool gpio_set_pin(gpio_ctrl_t *ctrl, uint32_t pin, bool level);

/* Sample an input pin into level_out. */
bool gpio_get_pin(const gpio_ctrl_t *ctrl, uint32_t pin, bool *level_out);

#endif /* GPIO_H */
