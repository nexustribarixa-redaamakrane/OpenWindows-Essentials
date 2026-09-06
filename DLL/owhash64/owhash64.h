/*
 * owhash64.h - OpenWindows Hash Functions Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWHASH64_H
#define OWHASH64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
void owhash_sha256_init(void);
void owhash_sha256_update(void);
void owhash_sha256_final(void);
void owhash_md5(void);
void owhash_crc32c(void);

#endif /* OWHASH64_H */

