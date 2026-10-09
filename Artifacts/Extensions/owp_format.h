/*
 * owp_format.h - OpenWindows Package (.owp) Archive Header Format
 *
 * Multi-file payload container bundling drivers (.owc), libraries (.owd),
 * executables (.owx), manifests (.owk), and resources (.owr) with 512-byte
 * sector alignment for direct VIP block reading and Fletcher/CRC32c verification.
 *
 * Header size: exactly 192 bytes. C99 freestanding.
 */

#ifndef OWP_FORMAT_H
#define OWP_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWP_MAGIC               0x4F575031u /* "OWP1" */
#define OWP_HEADER_SIZE         192u
#define OWP_FORMAT_VERSION      0x0001u
#define OWP_MAX_ENTRIES         128u

typedef struct {
    uint32_t name_offset;       /* Offset to relative path in strings*/
    uint32_t flags;             /* Compressed, executable, system    */
    uint64_t file_offset;       /* Byte offset in .owp image (512-al)*/
    uint64_t uncompressed_size; /* Raw byte size                     */
    uint64_t compressed_size;   /* Stored byte size                  */
    uint32_t checksum_crc32c;   /* Payload CRC32c                    */
    uint32_t reserved;          /* Must be zero                      */
} owp_file_entry_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* OWP_MAGIC (0x4F575031)   */
    /* 0x04 */ uint16_t format_version;       /* 0x0001                   */
    /* 0x06 */ uint16_t header_size;          /* 192 bytes                */
    /* 0x08 */ uint32_t package_size;         /* Total package size (B)   */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0xBF*/
    /* 0x10 */ uint32_t entry_count;          /* Number of files stored   */
    /* 0x14 */ uint32_t table_offset;         /* Offset to file table     */
    /* 0x18 */ uint32_t string_table_offset;  /* Offset to string table   */
    /* 0x1C */ uint32_t string_table_size;    /* Size of string table     */
    /* 0x20 */ uint64_t timestamp;            /* Build timestamp          */
    /* 0x28 */ uint32_t package_flags;        /* Boot, driver pack, user  */
    /* 0x2C */ uint32_t target_arch;          /* Target CPU architecture  */
    /* 0x30 */ uint32_t package_checksum;     /* Full archive CRC32c      */
    /* 0x34 */ uint32_t padding[47];          /* Pad to 192 bytes         */
} owp_header_t;

static inline bool owp_header_valid(const owp_header_t *h)
{
    if (!h) return false;
    if (h->magic != OWP_MAGIC) return false;
    if (h->header_size != OWP_HEADER_SIZE) return false;
    if (h->entry_count > OWP_MAX_ENTRIES) return false;
    return true;
}

#endif /* OWP_FORMAT_H */
