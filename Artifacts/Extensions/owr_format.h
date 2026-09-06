/*
 * owr_format.h - OpenWindows Resource Container (.owr) Header Format
 *
 * Defines binary resource archives bundling SuperUnicode string tables,
 * icons, cursors, raw bitmaps, and .suf fonts.
 *
 * Header size: exactly 128 bytes. C99 freestanding.
 */

#ifndef OWR_FORMAT_H
#define OWR_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWR_MAGIC               0x4F575231u /* "OWR1" */
#define OWR_HEADER_SIZE         128u
#define OWR_FORMAT_VERSION      0x0001u
#define OWR_MAX_RESOURCES       256u

/* Resource Types */
#define OWR_TYPE_STRING_TABLE   0x01u       /* SUTF-8 string catalog     */
#define OWR_TYPE_FONT_SUF       0x02u       /* SuperUnicode Font (.suf)  */
#define OWR_TYPE_ICON           0x03u       /* ARGB32 icon               */
#define OWR_TYPE_CURSOR         0x04u       /* Hardware / software cursor*/
#define OWR_TYPE_BITMAP         0x05u       /* Uncompressed bitmap       */
#define OWR_TYPE_RAW_BLOB       0x06u       /* Raw binary payload        */

typedef struct {
    uint32_t resource_id;       /* Numeric resource ID               */
    uint16_t type;              /* OWR_TYPE_* constant               */
    uint16_t flags;             /* Resource attributes               */
    uint64_t offset;            /* File offset to payload            */
    uint64_t size;              /* Byte size of payload              */
    uint32_t checksum;          /* CRC32c of resource data           */
    uint32_t name_offset;       /* String table offset (or 0)        */
} owr_entry_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* OWR_MAGIC (0x4F575231)   */
    /* 0x04 */ uint16_t format_version;       /* 0x0001                   */
    /* 0x06 */ uint16_t header_size;          /* 128 bytes                */
    /* 0x08 */ uint32_t container_size;       /* Full container size (B)  */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0x7F*/
    /* 0x10 */ uint32_t resource_count;       /* Total resources          */
    /* 0x14 */ uint32_t table_offset;         /* Offset to owr_entry_t    */
    /* 0x18 */ uint32_t string_table_offset;  /* Offset to name strings   */
    /* 0x1C */ uint32_t string_table_size;    /* Size of string table     */
    /* 0x20 */ uint64_t timestamp;            /* Build timestamp          */
    /* 0x28 */ uint32_t container_checksum;   /* Full archive CRC32c      */
    /* 0x2C */ uint32_t padding[13];          /* Pad to 128 bytes         */
} owr_header_t;

static inline bool owr_header_valid(const owr_header_t *h)
{
    if (!h) return false;
    if (h->magic != OWR_MAGIC) return false;
    if (h->header_size != OWR_HEADER_SIZE) return false;
    if (h->resource_count > OWR_MAX_RESOURCES) return false;
    return true;
}

#endif /* OWR_FORMAT_H */
