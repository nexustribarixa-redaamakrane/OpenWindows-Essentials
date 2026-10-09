/*
 * owpkg_format.h - OpenWindows Package Archive binary format specification
 *
 * C99 freestanding.
 */

#ifndef OWPKG_FORMAT_H
#define OWPKG_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWPKG_MAGIC   0x4F57504Bu
#define OWPKG_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} owpkg_header_t;

#endif /* OWPKG_FORMAT_H */
