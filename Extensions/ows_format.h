/*
 * ows_format.h - OpenWindows Symbol & Debug Map (.ows) Header Format
 *
 * Defines binary symbol tables and function address maps for fast,
 * zero-allocation stack trace unwinding during BANcode panics and
 * diagnostic trace logging.
 *
 * Header size: exactly 128 bytes. C99 freestanding.
 */

#ifndef OWS_FORMAT_H
#define OWS_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWS_MAGIC               0x4F575331u /* "OWS1" */
#define OWS_HEADER_SIZE         128u
#define OWS_FORMAT_VERSION      0x0001u
#define OWS_MAX_SYMBOLS         4096u

typedef struct {
    uint64_t start_rva;         /* Relative virtual address start    */
    uint32_t size;              /* Function / symbol byte size       */
    uint32_t name_offset;       /* String table offset               */
    uint32_t file_offset;       /* Source file string offset         */
    uint32_t line_number;       /* Source line number                */
} ows_symbol_record_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* OWS_MAGIC (0x4F575331)   */
    /* 0x04 */ uint16_t format_version;       /* 0x0001                   */
    /* 0x06 */ uint16_t header_size;          /* 128 bytes                */
    /* 0x08 */ uint32_t map_size;             /* Total byte size          */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0x7F*/
    /* 0x10 */ uint32_t symbol_count;         /* Number of symbol records */
    /* 0x14 */ uint32_t table_offset;         /* Offset to records        */
    /* 0x18 */ uint32_t string_table_offset;  /* Offset to string table   */
    /* 0x1C */ uint32_t string_table_size;    /* String table size (B)    */
    /* 0x20 */ uint64_t target_module_crc;    /* CRC32c of paired binary  */
    /* 0x28 */ uint64_t timestamp;            /* Map build timestamp      */
    /* 0x30 */ uint32_t full_checksum;        /* Full file CRC32c         */
    /* 0x34 */ uint32_t padding[19];          /* Pad to 128 bytes         */
} ows_header_t;

static inline bool ows_header_valid(const ows_header_t *h)
{
    if (!h) return false;
    if (h->magic != OWS_MAGIC) return false;
    if (h->header_size != OWS_HEADER_SIZE) return false;
    if (h->symbol_count > OWS_MAX_SYMBOLS) return false;
    return true;
}

#endif /* OWS_FORMAT_H */
