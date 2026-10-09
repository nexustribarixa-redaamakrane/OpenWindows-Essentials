/*
 * owfwup_format.h - OpenWindows Firmware Update Package binary format specification
 *
 * C99 freestanding.
 */

#ifndef OWFWUP_FORMAT_H
#define OWFWUP_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWFWUP_MAGIC   0x4F574655u
#define OWFWUP_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} owfwup_header_t;

#endif /* OWFWUP_FORMAT_H */
