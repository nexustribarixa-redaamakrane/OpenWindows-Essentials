/*
 * gpio.h - OpenWindows GPIO Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void gpio_init(void);
void gpio_set_pin(void);
void gpio_get_pin(void);
void gpio_set_direction(void);

#endif /* GPIO_H */

