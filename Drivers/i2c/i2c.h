/*
 * i2c.h - OpenWindows I2C Bus Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void i2c_init(void);
void i2c_read(void);
void i2c_write(void);
void i2c_scan_bus(void);

#endif /* I2C_H */

