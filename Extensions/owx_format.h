/*
 * owx_format.h - OpenWindows Native Executable (.owx) Binary Header Format
 *
 * Defines the fixed binary layout for native userland executables, system
 * services, and core utilities in the OpenWindows ecosystem. All .owx images
 * begin with this header at offset 0x0.
 *
 * Alignment: All fields are naturally aligned. The header occupies exactly
 * 256 bytes. Any trailing padding must be zero-filled.
 *
 * C99 freestanding - zero dynamic heap allocation.
 */

#ifndef OWX_FORMAT_H
#define OWX_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Magic & Versioning                                                */
/* ------------------------------------------------------------------ */

#define OWX_MAGIC               0x4F575831u /* "OWX1" in little-endian */
#define OWX_HEADER_SIZE         256u        /* Fixed header size (bytes) */
#define OWX_FORMAT_VERSION      0x0001u     /* Version 1.0               */
#define OWX_MAX_SECTIONS        64u         /* Maximum loadable sections */
#define OWX_MAX_IMPORTS         128u        /* Maximum DLL dependencies  */
#define OWX_MAX_TLS_SLOTS       32u         /* Maximum static TLS slots  */

/* ------------------------------------------------------------------ */
/*  Subsystem Types                                                   */
/* ------------------------------------------------------------------ */

#define OWX_SUBSYSTEM_UNKNOWN    0x00u       /* Undefined subsystem       */
#define OWX_SUBSYSTEM_NATIVE     0x01u       /* Native kernel service     */
#define OWX_SUBSYSTEM_CONSOLE    0x02u       /* Character / shell console */
#define OWX_SUBSYSTEM_GUI        0x03u       /* Cairo / OWUI Windowed     */
#define OWX_SUBSYSTEM_BOOT       0x04u       /* Early boot init utility   */
#define OWX_SUBSYSTEM_RECOVERY   0x05u       /* Sentinel recovery tool    */

/* ------------------------------------------------------------------ */
/*  Executable Flags                                                  */
/* ------------------------------------------------------------------ */

#define OWX_FLAG_NONE           0x00000000u
#define OWX_FLAG_PRIVILEGED     0x00000001u /* Requires ring-0 / driver access */
#define OWX_FLAG_LARGE_PAGES    0x00000002u /* Prefer 2 MiB / 1 GiB pages     */
#define OWX_FLAG_NO_DYNAMIC_REL 0x00000004u /* Fully pre-relocated at base     */
#define OWX_FLAG_SUPERUNICODE   0x00000008u /* Native SuperUnicode manifest    */
#define OWX_FLAG_SENTINEL_TRAP  0x00000010u /* Trapped under sentinel recovery */
#define OWX_FLAG_DMA_DIRECT     0x00000020u /* Direct physical DMA mapped     */

/* ------------------------------------------------------------------ */
/*  Section Types                                                     */
/* ------------------------------------------------------------------ */

#define OWX_SECTION_CODE        0x01u       /* Executable instructions   */
#define OWX_SECTION_RDATA       0x02u       /* Read-only constants       */
#define OWX_SECTION_DATA        0x03u       /* Read-write initialized    */
#define OWX_SECTION_BSS         0x04u       /* Zero-initialized space    */
#define OWX_SECTION_RELOC       0x05u       /* Relocation fixup table    */
#define OWX_SECTION_IMPORT      0x06u       /* Import table (.owd deps)  */
#define OWX_SECTION_EXPORT      0x07u       /* Export table (if any)     */
#define OWX_SECTION_TLS         0x08u       /* Thread Local Storage init */
#define OWX_SECTION_RESOURCE    0x09u       /* Embedded .owr resources   */
#define OWX_SECTION_SENTINEL    0x0Au       /* Embedded recovery rules   */

/* ------------------------------------------------------------------ */
/*  Section Header (32 bytes each)                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t type;              /* OWX_SECTION_* constant            */
    uint32_t flags;             /* Section protection & caching      */
    uint64_t file_offset;       /* Offset in .owx image file         */
    uint64_t virtual_addr;      /* Target virtual load address       */
    uint64_t size;              /* Raw uncompressed byte size        */
    uint32_t checksum;          /* CRC32c of section payload         */
    uint32_t reserved;          /* Must be zero                      */
} owx_section_entry_t;

/* ------------------------------------------------------------------ */
/*  Import Dependency Entry (32 bytes each)                           */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t name_offset;       /* Offset in string table for .owd   */
    uint16_t min_version;       /* Minimum compatible library ver    */
    uint16_t max_version;       /* Maximum compatible library ver    */
    uint32_t symbol_count;      /* Number of imported symbols        */
    uint32_t symbol_table_off;  /* Offset to symbol lookup entries   */
    uint32_t iat_offset;        /* Import Address Table fixup offset */
    uint32_t checksum;          /* CRC32c of expected library export */
    uint64_t reserved;          /* Must be zero                      */
} owx_import_entry_t;

/* ------------------------------------------------------------------ */
/*  .owx Binary Header (256 bytes, at offset 0x0)                     */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 0x00 */ uint32_t magic;                /* Must be OWX_MAGIC (0x4F575831) */
    /* 0x04 */ uint16_t format_version;       /* OWX_FORMAT_VERSION (0x0001)   */
    /* 0x06 */ uint16_t header_size;          /* Must be OWX_HEADER_SIZE (256) */
    /* 0x08 */ uint32_t image_size;           /* Total image size in bytes     */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0xFF    */
    /* 0x10 */ uint64_t entry_point;          /* Virtual address of entry      */
    /* 0x18 */ uint64_t preferred_base;       /* Preferred virtual load base   */
    /* 0x20 */ uint64_t stack_reserve;        /* Stack reserve size in bytes   */
    /* 0x28 */ uint64_t stack_commit;         /* Stack initial commit in bytes */
    /* 0x30 */ uint64_t heap_reserve;         /* Fixed heap reserve size (B)   */
    /* 0x38 */ uint64_t heap_commit;          /* Fixed heap initial commit (B) */
    /* 0x40 */ uint32_t section_count;        /* Number of section entries     */
    /* 0x44 */ uint32_t import_count;         /* Number of import entries      */
    /* 0x48 */ uint32_t string_table_size;    /* String table size in bytes    */
    /* 0x4C */ uint8_t  subsystem;            /* OWX_SUBSYSTEM_* constant      */
    /* 0x4D */ uint8_t  target_arch;          /* Target CPU architecture       */
    /* 0x4E */ uint8_t  target_subarch;       /* Architecture variant          */
    /* 0x4F */ uint8_t  alignment_log2;       /* Page alignment power (12=4KB) */
    /* 0x50 */ uint32_t flags;                /* OWX_FLAG_* bitmask            */
    /* 0x54 */ uint32_t tls_index;            /* TLS slot directory index      */
    /* 0x58 */ uint64_t timestamp;            /* Build timestamp (UTC seconds) */
    /* 0x60 */ uint32_t section_table_offset; /* Offset to section table       */
    /* 0x64 */ uint32_t import_table_offset;  /* Offset to import table        */
    /* 0x68 */ uint32_t string_table_offset;  /* Offset to string table        */
    /* 0x6C */ uint32_t reloc_table_offset;   /* Offset to relocation table    */
    /* 0x70 */ uint32_t resource_offset;      /* Offset to embedded .owr data  */
    /* 0x74 */ uint32_t resource_size;        /* Size of embedded resource     */
    /* 0x78 */ uint32_t debug_offset;         /* Offset to .ows debug map      */
    /* 0x7C */ uint32_t debug_size;           /* Size of debug map data        */
    /* 0x80 */ uint32_t sentinel_bancode;     /* BANcode panic hook code       */
    /* 0x84 */ uint32_t sentinel_trap_slot;   /* Trap table index (0..14)      */
    /* 0x88 */ uint64_t sentinel_recovery_ep; /* Recovery fallback entry point */
    /* 0x90 */ uint32_t image_checksum;       /* CRC32c of entire image file   */
    /* 0x94 */ uint32_t padding[27];          /* Zero-padded to 256 bytes      */
} owx_header_t;

/* ------------------------------------------------------------------ */
/*  Validation Helper (Freestanding inline)                           */
/* ------------------------------------------------------------------ */

static inline bool owx_header_valid(const owx_header_t *h)
{
    if (!h) return false;
    if (h->magic != OWX_MAGIC) return false;
    if (h->header_size != OWX_HEADER_SIZE) return false;
    if (h->format_version != OWX_FORMAT_VERSION) return false;
    if (h->section_count > OWX_MAX_SECTIONS) return false;
    if (h->import_count > OWX_MAX_IMPORTS) return false;
    if (h->subsystem > OWX_SUBSYSTEM_RECOVERY) return false;
    if (h->sentinel_trap_slot > 14u) return false;
    return true;
}

#endif /* OWX_FORMAT_H */
