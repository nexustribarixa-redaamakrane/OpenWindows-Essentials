/*
 * smbios.h - OpenWindows SMBIOS Table Parser (.owc)
 *
 * Scans caller-provided memory for a 32-bit ("_SM_") or 64-bit
 * ("_SM3_") SMBIOS entry point, validates its checksum, follows the
 * structure table pointer and walks the structure table by type.
 *
 * C99 freestanding. Zero dynamic heap allocation; the caller provides
 * the memory region that contains the entry point.
 */

#ifndef SMBIOS_H
#define SMBIOS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define SMBIOS_EPS_ANCHOR      "_SM_"    /* 32-bit entry point anchor */
#define SMBIOS_EPS3_ANCHOR     "_SM3_"   /* 64-bit entry point anchor */
#define SMBIOS_INTERMEDIATE    "_DMI_"   /* intermediate anchor       */
#define SMBIOS_EPS_LEN         0x1Fu     /* minimum 32-bit EPS length */
#define SMBIOS_EPS3_MIN_LEN    0x18u     /* minimum 64-bit EPS length */
#define SMBIOS_STRUCT_END      0x7Fu     /* end-of-table structure type */

/* SMBIOS 2.1+ 32-bit entry point structure. */
typedef struct __attribute__((packed)) {
    char     anchor[4];             /* "_SM_"                         */
    uint8_t  checksum;              /* sum of 'length' bytes == 0     */
    uint8_t  length;                /* entry point length             */
    uint8_t  major_version;         /* SMBIOS major version           */
    uint8_t  minor_version;         /* SMBIOS minor version           */
    uint16_t max_structure_size;    /* largest structure size         */
    uint8_t  entry_point_revision;
    uint8_t  formatted_area[5];
    char     intermediate_anchor[5]; /* "_DMI_"                       */
    uint8_t  intermediate_checksum;
    uint16_t structure_table_length;
    uint32_t structure_table_address;
    uint16_t number_of_structures;
    uint8_t  bcd_revision;
} smbios_eps_t;

/* SMBIOS 3.0 64-bit entry point structure. */
typedef struct __attribute__((packed)) {
    char     anchor[5];             /* "_SM3_"                        */
    uint8_t  checksum;              /* sum of 'length' bytes == 0     */
    uint8_t  length;                /* entry point length             */
    uint8_t  major_version;         /* SMBIOS major version           */
    uint8_t  minor_version;         /* SMBIOS minor version           */
    uint8_t  docrev;                /* doc revision                   */
    uint8_t  entry_point_revision;
    uint8_t  reserved;
    uint32_t structure_table_max_size;
    uint64_t structure_table_address;
} smbios_eps3_t;

/* Common header at the start of every SMBIOS structure. */
typedef struct __attribute__((packed)) {
    uint8_t  type;      /* structure type, 0x7F marks end of table    */
    uint8_t  length;    /* length of the formatted area in bytes      */
    uint16_t handle;    /* structure handle                           */
} smbios_header_t;

/* Scan 'search_base' (16-byte aligned) for a valid entry point. */
bool smbios_init(const void *search_base, size_t search_length);

/* Return the first structure of the given type, or NULL. */
const smbios_header_t *smbios_find_table(uint8_t type);

/* Resolve a 1-based string index against a structure's string pool. */
const char *smbios_get_string(const smbios_header_t *header, uint8_t index);

/* Packed version as (major << 8) | minor. */
uint32_t smbios_version(void);

/* Generic checksum helper: 'length' bytes must sum to zero (mod 256). */
bool smbios_checksum_ok(const void *data, size_t length);

/* Parser state read-back. */
const uint8_t *smbios_table_address(void);
size_t         smbios_table_length(void);
uint16_t       smbios_structure_count(void);
bool           smbios_is_initialized(void);

#endif /* SMBIOS_H */
