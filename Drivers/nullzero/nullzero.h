/*
 * nullzero.h - OpenWindows Pseudo-Devices Driver (/dev/null, /dev/zero) (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef NULLZERO_H
#define NULLZERO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

size_t null_read(void *buf, size_t len);
size_t null_write(const void *buf, size_t len);
size_t zero_read(void *buf, size_t len);
size_t zero_write(const void *buf, size_t len);

#endif /* NULLZERO_H */
