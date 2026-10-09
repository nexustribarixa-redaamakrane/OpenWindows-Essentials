/*
 * spi.h - OpenWindows SPI Bus Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void spi_init(void);
void spi_transfer(void);
void spi_set_mode(void);
void spi_set_speed(void);

#endif /* SPI_H */

