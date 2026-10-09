/*
 * owdump_format.h - OpenWindows Memory Dump binary format specification
 *
 * C99 freestanding.
 */

#ifndef OWDUMP_FORMAT_H
#define OWDUMP_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWDUMP_MAGIC   0x4F574450u
#define OWDUMP_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} owdump_header_t;

#endif /* OWDUMP_FORMAT_H */
