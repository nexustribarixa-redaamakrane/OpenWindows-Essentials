/*
 * acpitbl.h - OpenWindows ACPI Table Parser (.owc)
 *
 * Locates and validates the ACPI Root System Description Pointer (RSDP),
 * selects the RSDT (ACPI 1.0, 32-bit pointers) or the XSDT (ACPI 2.0+,
 * 64-bit pointers), and walks the resulting table directory by the
 * 4-character signature of each system description table.
 *
 * The caller supplies the address of the RSDP; nothing is allocated.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef ACPITBL_H
#define ACPITBL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define ACPITBL_RSDP_SIG_LEN   8u   /* "RSD PTR "                   */
#define ACPITBL_SIG_LEN        4u   /* table signature length       */
#define ACPITBL_RSDP_V1_LEN    20u  /* ACPI 1.0 RSDP checksum range */
#define ACPITBL_RSDP_MIN_LEN   36u  /* ACPI 2.0+ RSDP total length  */

#define ACPITBL_RSDT_SIG       "RSDT"  /* 32-bit table pointers     */
#define ACPITBL_XSDT_SIG       "XSDT"  /* 64-bit table pointers     */

/* Root System Description Pointer, revision 0 and 2 (packed on the wire). */
typedef struct __attribute__((packed)) {
    char     signature[8];      /* "RSD PTR "                            */
    uint8_t  checksum;          /* sum of first 20 bytes == 0            */
    char     oem_id[6];         /* OEM identifier                        */
    uint8_t  revision;          /* 0 = ACPI 1.0, >= 2 = ACPI 2.0+        */
    uint32_t rsdt_address;      /* 32-bit physical address of the RSDT   */
    /* Extended fields, valid only when revision >= 2. */
    uint32_t length;            /* total RSDP length in bytes            */
    uint64_t xsdt_address;      /* 64-bit physical address of the XSDT   */
    uint8_t  extended_checksum; /* sum over 'length' bytes == 0          */
    uint8_t  reserved[3];       /* reserved, must be zero                */
} acpitbl_rsdp_t;

/* Common header at the start of every ACPI system description table. */
typedef struct __attribute__((packed)) {
    char     signature[4];      /* e.g. "RSDT", "XSDT", "FACP"           */
    uint32_t length;            /* total table length in bytes           */
    uint8_t  revision;          /* table revision                        */
    uint8_t  checksum;          /* sum over 'length' bytes == 0          */
    char     oem_id[6];         /* OEM identifier                        */
    char     oem_table_id[8];   /* OEM table identifier                  */
    uint32_t oem_revision;      /* OEM revision number                   */
    uint32_t creator_id;        /* creator vendor identifier             */
    uint32_t creator_revision;  /* creator revision number               */
} acpitbl_header_t;

/* Summary of a validated RSDP and its selected root table. */
typedef struct {
    bool     rsdp_valid;        /* RSDP signatures/checksums verified    */
    bool     xsdt;              /* true: XSDT (64-bit), false: RSDT      */
    uint8_t  revision;          /* ACPI revision reported by the RSDP    */
    uint8_t  reserved[3];       /* padding                               */
    uint32_t entry_count;       /* table pointers in the root SDT        */
    uint64_t root_address;      /* address of the RSDT or XSDT           */
    uint64_t rsdp_address;      /* address of the RSDP                   */
} acpitbl_info_t;

/* Initialise the parser from the address of an RSDP. */
bool acpitbl_init(const void *rsdp_address);

/* Validate an RSDP, check its extended checksum and follow its pointer. */
bool acpitbl_validate_rsdp(const void *rsdp_address, acpitbl_info_t *info);

/* Find a table by its 4-byte signature; returns NULL when absent. */
const acpitbl_header_t *acpitbl_find_table(const char *signature);

/* Number of table pointers held by the active root table. */
uint32_t acpitbl_entry_count(void);

/* Header of the active RSDT/XSDT, or NULL before a successful init. */
const acpitbl_header_t *acpitbl_root_table(void);

/* Generic ACPI checksum: 'length' bytes must sum to zero (mod 256). */
bool acpitbl_checksum_ok(const void *data, size_t length);

#endif /* ACPITBL_H */
