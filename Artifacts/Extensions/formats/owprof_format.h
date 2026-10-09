/*
 * owprof_format.h - OpenWindows Profiler Data binary format specification
 *
 * C99 freestanding.
 */

#ifndef OWPROF_FORMAT_H
#define OWPROF_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWPROF_MAGIC   0x4F575046u
#define OWPROF_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} owprof_header_t;

#endif /* OWPROF_FORMAT_H */
