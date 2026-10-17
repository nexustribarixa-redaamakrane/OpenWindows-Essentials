/*
 * owio64.h - OpenWindows Port I/O Abstraction Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 *
 * Thin typed wrappers over the x86 port address space. All helpers are
 * implemented with inline assembly so no libc is required.
 */

#ifndef OWIO64_H
#define OWIO64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Standard x86 delay/scratch port (unmapped on most chipsets). */
#define OWIO_DELAY_PORT 0x80u

/* Mark the port subsystem ready (pure latch; no hardware touched). */
bool owio_init(void);

/* True once owio_init() has run. */
bool owio_is_ready(void);

/* Read an 8-bit value from `port`. */
uint8_t owio_inb(uint16_t port);

/* Write an 8-bit value to `port`. */
void owio_outb(uint16_t port, uint8_t value);

/* Read a 16-bit value from `port`. */
uint16_t owio_inw(uint16_t port);

/* Write a 16-bit value to `port`. */
void owio_outw(uint16_t port, uint16_t value);

/* Read a 32-bit value from `port`. */
uint32_t owio_ind(uint16_t port);

/* Write a 32-bit value to `port`. */
void owio_outd(uint16_t port, uint32_t value);

/* Alias for the 32-bit read (native 64-bit build spelling). */
uint32_t owio_inl(uint16_t port);

/* Alias for the 32-bit write (native 64-bit build spelling). */
void owio_outl(uint16_t port, uint32_t value);

/* Small I/O delay: a write to the unmapped delay port. */
void owio_wait(void);

#endif /* OWIO64_H */
