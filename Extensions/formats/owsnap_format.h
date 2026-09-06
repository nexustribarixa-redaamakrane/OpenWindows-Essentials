/*
 * owsnap_format.h - OpenWindows Snapshot/Checkpoint binary format specification
 *
 * C99 freestanding.
 */

#ifndef OWSNAP_FORMAT_H
#define OWSNAP_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWSNAP_MAGIC   0x4F575350u
#define OWSNAP_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} owsnap_header_t;

#endif /* OWSNAP_FORMAT_H */
