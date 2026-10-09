/*
 * owimg_format.h - OpenWindows Disk Image binary format specification
 *
 * C99 freestanding.
 */

#ifndef OWIMG_FORMAT_H
#define OWIMG_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWIMG_MAGIC   0x4F574947u
#define OWIMG_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} owimg_header_t;

#endif /* OWIMG_FORMAT_H */
