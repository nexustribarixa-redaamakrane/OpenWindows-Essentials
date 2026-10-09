/*
 * owzip64.h - OpenWindows Data Compression Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWZIP64_H
#define OWZIP64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owzip_compress(void);
void owzip_decompress(void);
void owzip_crc32(void);
void owzip_adler32(void);

#endif /* OWZIP64_H */

