/*
 * ktext_format.h - OpenWindows Kernel Localized Text Catalog (.ktext) Header
 *
 * Provides a compact, zero-allocation binary catalog for kernel error messages,
 * boot status strings, and driver diagnostic alerts in SuperUnicode/SUTF-8.
 *
 * Header size: exactly 128 bytes. C99 freestanding.
 */

#ifndef KTEXT_FORMAT_H
#define KTEXT_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KTEXT_MAGIC             0x4B545854u /* "KTXT" */
#define KTEXT_HEADER_SIZE       128u
#define KTEXT_FORMAT_VERSION    0x0001u
#define KTEXT_MAX_STRINGS       512u

typedef struct {
    uint32_t message_id;        /* Unique message identifier         */
    uint32_t string_offset;     /* Offset into string table          */
    uint16_t length;            /* String byte length                */
    uint16_t encoding;          /* 0=ASCII, 1=SUTF-8, 2=SUTF-16      */
    uint32_t checksum;          /* CRC32c of string content          */
} ktext_entry_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* KTEXT_MAGIC (0x4B545854)  */
    /* 0x04 */ uint16_t format_version;       /* 0x0001                    */
    /* 0x06 */ uint16_t header_size;          /* 128 bytes                 */
    /* 0x08 */ uint32_t catalog_size;         /* Total catalog file size   */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0x7F*/
    /* 0x10 */ uint32_t message_count;        /* Number of messages stored */
    /* 0x14 */ uint32_t table_offset;         /* Offset to ktext_entry_t   */
    /* 0x18 */ uint32_t string_table_offset;  /* Offset to string buffer   */
    /* 0x1C */ uint32_t string_table_size;    /* Size of string buffer     */
    /* 0x20 */ uint32_t locale_code;          /* e.g. 0x656E (en), 0x6672  */
    /* 0x24 */ uint32_t reserved_0;           /* Zero                      */
    /* 0x28 */ uint64_t timestamp;            /* Catalog creation time     */
    /* 0x30 */ uint32_t full_checksum;        /* Full file CRC32c          */
    /* 0x34 */ uint32_t padding[19];          /* Pad to 128 bytes          */
} ktext_header_t;

static inline bool ktext_header_valid(const ktext_header_t *h)
{
    if (!h) return false;
    if (h->magic != KTEXT_MAGIC) return false;
    if (h->header_size != KTEXT_HEADER_SIZE) return false;
    if (h->message_count > KTEXT_MAX_STRINGS) return false;
    return true;
}

#endif /* KTEXT_FORMAT_H */
