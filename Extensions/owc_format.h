/*
 * owc_format.h - OpenWindows Component (.owc) Binary Header Format
 *
 * Defines the fixed binary layout for bare-metal hardware drivers,
 * kernel modules, and protocol arbiters. All .owc images begin with
 * this header at offset 0x0.
 *
 * Alignment: All fields are naturally aligned. The header occupies
 * exactly 256 bytes. Any trailing padding must be zero-filled.
 *
 * C99 freestanding - no dynamic allocation.
 */

#ifndef OWC_FORMAT_H
#define OWC_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Magic & Versioning                                                 */
/* ------------------------------------------------------------------ */

#define OWC_MAGIC           0x4F574331  /* "OWC1" in little-endian    */
#define OWC_HEADER_SIZE     256u        /* Fixed header size in bytes  */
#define OWC_FORMAT_VERSION  0x0001u     /* Major.Minor = 1.0           */
#define OWC_MAX_SECTIONS    32u         /* Max loadable sections       */
#define OWC_MAX_EXPORTS     128u        /* Max export table entries    */

/* ------------------------------------------------------------------ */
/*  Driver Initialization Flags                                       */
/* ------------------------------------------------------------------ */

#define OWC_FLAG_NONE           0x00000000u
#define OWC_FLAG_PNP_AWARE      0x00000001u  /* Driver participates    */
                                             /* in PnP enumeration     */
#define OWC_FLAG_BOOT_DRIVER    0x00000002u  /* Loaded during early    */
                                             /* boot (pre-filesystem)  */
#define OWC_FLAG_FILTER_DRIVER  0x00000004u  /* Intercepts I/O below   */
                                             /* another driver layer   */
#define OWC_FLAG_DMA_COHERENT   0x00000008u  /* Requires coherent DMA  */
                                             /* mappings                */
#define OWC_FLAG_PRIVILEGED     0x00000010u  /* Requires ring-0 or     */
                                             /* equivalent access      */
#define OWC_FLAG_SENTINEL_AWARE 0x00000020u  /* Participates in        */
                                             /* sentinel recovery      */

/* ------------------------------------------------------------------ */
/*  Section Types                                                     */
/* ------------------------------------------------------------------ */

#define OWC_SECTION_CODE        0x01u   /* Executable code section     */
#define OWC_SECTION_DATA        0x02u   /* Initialized data            */
#define OWC_SECTION_BSS         0x03u   /* Zero-initialized data       */
#define OWC_SECTION_RELOC       0x04u   /* Relocation table            */
#define OWC_SECTION_EXPORT      0x05u   /* Export symbol table         */
#define OWC_SECTION_IMPORT      0x06u   /* Import dependency table     */
#define OWC_SECTION_DEBUG       0x07u   /* Debug / diagnostic info     */
#define OWC_SECTION_SENTINEL    0x08u   /* Embedded sentinel directives*/

/* ------------------------------------------------------------------ */
/*  Return / Status Codes                                             */
/* ------------------------------------------------------------------ */

typedef uint32_t owc_status_t;

#define OWC_OK                          0x00000000u  /* Success              */
/* B+ Fatal */
#define OWC_ERR_INVALID_MAGIC           0x0011A000u  /* Bad magic number     */
#define OWC_ERR_UNSUPPORTED_VERSION     0x0011A001u  /* Unsupported format   */
#define OWC_ERR_HEADER_CRC              0x0011A002u  /* Header CRC corrupt   */
#define OWC_ERR_SECTION_CRC             0x0011A003u  /* Section CRC corrupt  */
#define OWC_ERR_TOO_MANY_SECTIONS       0x0011A004u  /* Section limit hit    */
#define OWC_ERR_ENTRY_NOT_FOUND         0x0011A005u  /* Entry not found      */
#define OWC_ERR_DEPENDENCY_MISSING      0x0011A006u  /* Missing dependency   */
#define OWC_ERR_UNALIGNED_ADDRESS       0x0011A007u  /* Unaligned address    */
#define OWC_ERR_SENTINEL_VIOLATION      0x0011A008u  /* Sentinel violation   */

/* ------------------------------------------------------------------ */
/*  Section Header (32 bytes each)                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t type;          /* OWC_SECTION_* constant                  */
    uint32_t flags;         /* Section-specific flags                  */
    uint64_t offset;        /* Byte offset from start of image         */
    uint64_t virtual_addr;  /* Target load address                     */
    uint64_t size;          /* Section size in bytes (uncompressed)    */
    uint32_t checksum;      /* CRC32c of section payload               */
    uint32_t reserved;      /* Must be zero                            */
} owc_section_entry_t;

/* ------------------------------------------------------------------ */
/*  Export Table Entry (32 bytes each)                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t address;       /* Absolute address of exported symbol     */
    uint32_t name_offset;   /* Offset into string table (from          */
                            /* start of string table section)          */
    uint16_t ordinal;       /* Stable export ordinal                   */
    uint8_t  flags;         /* Export flags (see below)                */
    uint8_t  reserved;      /* Must be zero                            */
    uint32_t checksum;      /* CRC32c of the exported code/data        */
} owc_export_entry_t;

#define OWC_EXPORT_ORDINAL_STABLE  0x01u  /* Ordinal never changes     */
#define OWC_EXPORT_ORDINAL_HINT    0x02u  /* Hint table present        */

/* ------------------------------------------------------------------ */
/*  Driver Initialization Flags (at offset 0x70 in header)            */
/* ------------------------------------------------------------------ */

typedef enum {
    OWC_INIT_NONE           = 0x00u,   /* No special requirements     */
    OWC_INIT_REQUIRES_HTL   = 0x01u,   /* Needs HTL (htl.owd)        */
    OWC_INIT_REQUIRES_OWRP  = 0x02u,   /* Needs OWRP (owrp.owd)      */
    OWC_INIT_REQUIRES_VIP   = 0x04u,   /* Needs UniVIP/FVIP hooks     */
    OWC_INIT_REQUIRES_BANC  = 0x08u,   /* Needs BANcode subsystem     */
    OWC_INIT_REQUIRES_BOOT  = 0x10u,   /* Needs MBL handoff data      */
    OWC_INIT_REQUIRES_FONT  = 0x20u   /* Needs EFI CP437 font data   */
} owc_init_requirement_t;

/* ------------------------------------------------------------------ */
/*  .owc Binary Image Header (256 bytes, at offset 0x0)               */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 0x00 */ uint32_t magic;                /* Must be OWC_MAGIC          */
    /* 0x04 */ uint16_t format_version;       /* OWC_FORMAT_VERSION         */
    /* 0x06 */ uint16_t header_size;          /* Must be OWC_HEADER_SIZE    */
    /* 0x08 */ uint32_t image_size;           /* Total image size in bytes  */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0xFF */
    /* 0x10 */ uint64_t entry_point;          /* Offset to driver entry     */
    /* 0x18 */ uint64_t init_entry;           /* Offset to init function    */
    /* 0x20 */ uint64_t unload_entry;         /* Offset to unload function  */
    /* 0x28 */ uint32_t section_count;        /* Number of sections         */
    /* 0x2C */ uint32_t export_count;         /* Number of exports          */
    /* 0x30 */ uint32_t import_count;         /* Number of imports          */
    /* 0x34 */ uint32_t string_table_size;    /* Size of string table (B)   */
    /* 0x38 */ uint32_t init_flags;           /* owc_init_requirement_t bitfield */
    /* 0x3C */ uint32_t driver_flags;         /* OWC_FLAG_* bitfield        */
    /* 0x40 */ uint64_t timestamp;            /* Build timestamp (UTC secs) */
    /* 0x48 */ uint32_t target_arch;          /* Target architecture ID     */
    /* 0x4C */ uint32_t target_subarch;       /* Sub-architecture variant   */
    /* 0x50 */ uint64_t min_memory;           /* Minimum memory required    */
    /* 0x58 */ uint64_t max_memory;           /* Maximum memory usable      */
    /* 0x60 */ uint32_t io_version;           /* Minimum I/O subsystem ver  */
    /* 0x64 */ uint32_t vip_version;          /* Minimum VIP subsystem ver  */
    /* 0x68 */ uint32_t bancode_version;      /* Minimum BANcode subsystem  */
    /* 0x6C */ uint32_t reserved_0;           /* Must be zero               */
    /* 0x70 */ uint32_t sentinel_bancode;     /* BANcode for sentinel hook  */
    /* 0x74 */ uint32_t sentinel_trap_index;  /* Trap slot for recovery     */
    /* 0x78 */ uint64_t sentinel_entry;       /* Offset to sentinel handler */
    /* 0x80 */ uint32_t image_checksum;       /* CRC32c of full image       */
    /* 0x84 */ uint32_t import_table_offset;  /* Offset to import table     */
    /* 0x88 */ uint32_t section_table_offset; /* Offset to section table    */
    /* 0x8C */ uint32_t export_table_offset;  /* Offset to export table     */
    /* 0x90 */ uint32_t string_table_offset;  /* Offset to string table     */
    /* 0x94 */ uint32_t debug_offset;         /* Offset to debug info       */
    /* 0x98 */ uint32_t debug_size;           /* Size of debug info         */
    /* 0x9C */ uint32_t padding[28];          /* Pad to 256 bytes, zeros    */
} owc_header_t;

/* ------------------------------------------------------------------ */
/*  Architecture IDs                                                  */
/* ------------------------------------------------------------------ */

#define OWC_ARCH_UNKNOWN    0x00000000u
#define OWC_ARCH_X86        0x00000001u
#define OWC_ARCH_X86_64     0x00000002u
#define OWC_ARCH_ARM        0x00000003u
#define OWC_ARCH_ARM64      0x00000004u
#define OWC_ARCH_RISCV32    0x00000005u
#define OWC_ARCH_RISCV64    0x00000006u

/* ------------------------------------------------------------------ */
/*  BANcode Sentinel Integration                                       */
/*  (Cross-ref: BANcode kernel_inc/bancode/bancode_all.h)             */
/*  Sentinel recovery triggers on BANcode in B+ block                 */
/*  0x0011A000-0x0011A7FF. Trap slot indexed by                       */
/*  sentinel_trap_index maps into trap table                           */
/*  0x7FFFFFF0-0x7FFFFFFE.                                            */
/* ------------------------------------------------------------------ */

#define OWC_BANCODE_RANGE_LO    0x0011A000u
#define OWC_BANCODE_RANGE_HI    0x0011A7FFu
#define OWC_TRAP_BASE           0x7FFFFFF0u
#define OWC_TRAP_END            0x7FFFFFFEu
#define OWC_TRAP_SLOT_COUNT     15u

/* ------------------------------------------------------------------ */
/*  Convenience: Validate an .owc header in place                     */
/* ------------------------------------------------------------------ */

static inline bool owc_header_valid(const owc_header_t *h)
{
    if (!h) return false;
    if (h->magic != OWC_MAGIC) return false;
    if (h->header_size != OWC_HEADER_SIZE) return false;
    if (h->section_count > OWC_MAX_SECTIONS) return false;
    if (h->export_count > OWC_MAX_EXPORTS) return false;
    if (h->sentinel_trap_index >= OWC_TRAP_SLOT_COUNT) return false;
    if (h->sentinel_bancode < OWC_BANCODE_RANGE_LO ||
        h->sentinel_bancode > OWC_BANCODE_RANGE_HI) return false;
    return true;
}

#endif /* OWC_FORMAT_H */
