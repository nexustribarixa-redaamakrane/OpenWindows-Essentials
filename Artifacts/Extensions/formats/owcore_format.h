/*
 * owcore_format.h - OpenWindows Core Dump binary format specification
 *
 * C99 freestanding.
 */

#ifndef OWCORE_FORMAT_H
#define OWCORE_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWCORE_MAGIC   0x4F574352u
#define OWCORE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} owcore_header_t;

#endif /* OWCORE_FORMAT_H */
