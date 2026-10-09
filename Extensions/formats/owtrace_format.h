/*
 * owtrace_format.h - OpenWindows Execution Trace binary format specification
 *
 * C99 freestanding.
 */

#ifndef OWTRACE_FORMAT_H
#define OWTRACE_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWTRACE_MAGIC   0x4F575452u
#define OWTRACE_VERSION 1u

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t flags;
    uint32_t header_size;
    uint64_t total_size;
    uint64_t timestamp;
    uint32_t checksum;
    uint8_t  reserved[20];
} owtrace_header_t;

#endif /* OWTRACE_FORMAT_H */
