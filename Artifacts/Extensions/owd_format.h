/*
 * owd_format.h - OpenWindows Dynamic Link Library (.owd) Binary Header Format
 *
 * Defines the fixed binary layout for freestanding user/kernel dynamic
 * link libraries. All .owd images begin with this header at offset 0x0.
 *
 * Alignment: All fields are naturally aligned. The header occupies
 * exactly 192 bytes. Any trailing padding must be zero-filled.
 *
 * C99 freestanding - no dynamic allocation.
 */

#ifndef OWD_FORMAT_H
#define OWD_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Magic & Versioning                                                 */
/* ------------------------------------------------------------------ */

#define OWD_MAGIC           0x4F574431  /* "OWD1" in little-endian    */
#define OWD_HEADER_SIZE     192u        /* Fixed header size in bytes  */
#define OWD_FORMAT_VERSION  0x0001u     /* Major.Minor = 1.0           */
#define OWD_MAX_SYMBOLS     256u        /* Max symbol table entries    */
#define OWD_MAX_RELOC       64u         /* Max relocation entries      */
#define OWD_MAX_DEPENDS     8u          /* Max dependency libraries    */

/* ------------------------------------------------------------------ */
/*  Library Type Flags                                                */
/* ------------------------------------------------------------------ */

#define OWD_LIBTYPE_USER        0x00u  /* Userspace library           */
#define OWD_LIBTYPE_KERNEL      0x01u  /* Kernel-mode library         */
#define OWD_LIBTYPE_HYBRID      0x02u  /* Dual-mode (user + kernel)   */
#define OWD_LIBTYPE_BOOT        0x03u  /* Early boot library          */

/* ------------------------------------------------------------------ */
/*  Symbol Visibility                                                 */
/* ------------------------------------------------------------------ */

#define OWD_SYM_LOCAL       0x00u      /* Not exported                */
#define OWD_SYM_GLOBAL      0x01u      /* Globally visible            */
#define OWD_SYM_WEAK        0x02u      /* Weak symbol (overridable)   */
#define OWD_SYM_KERNEL      0x03u      /* Kernel-only export          */

/* ------------------------------------------------------------------ */
/*  Relocation Types                                                  */
/* ------------------------------------------------------------------ */

#define OWD_RELOC_ABS32     0x01u      /* 32-bit absolute address     */
#define OWD_RELOC_ABS64     0x02u      /* 64-bit absolute address     */
#define OWD_RELOC_REL32     0x03u      /* 32-bit PC-relative          */
#define OWD_RELOC_REL64     0x04u      /* 64-bit PC-relative          */
#define OWD_RELOC_BASE_REF  0x05u      /* Base address reference      */
#define OWD_RELOC_HTLCALL   0x06u      /* HTL thunk relocation        */
#define OWD_RELOC_OWRPCALL  0x07u      /* OWRP gate call relocation   */

/* ------------------------------------------------------------------ */
/*  Return / Status Codes                                             */
/* ------------------------------------------------------------------ */

typedef uint32_t owd_status_t;

#define OWD_OK                          0x00000000u  /* Success              */
/* B+ Fatal */
#define OWD_ERR_INVALID_MAGIC           0x0011A000u  /* Bad magic number     */
#define OWD_ERR_UNSUPPORTED_VERSION     0x0011A001u  /* Unsupported format   */
#define OWD_ERR_HEADER_CRC              0x0011A002u  /* Header CRC corrupt   */
#define OWD_ERR_SYMBOL_CRC              0x0011A003u  /* Symbol table CRC     */
#define OWD_ERR_RELOC_FAILED            0x0011A004u  /* Relocation failed    */
#define OWD_ERR_DEPENDENCY_MISSING      0x0011A005u  /* Missing dependency   */
#define OWD_ERR_SYMBOL_CONFLICT         0x0011A006u  /* Symbol conflict      */
#define OWD_ERR_BASE_CONFLICT           0x0011A007u  /* Base addr conflict   */
#define OWD_ERR_INSUFFICIENT_SPACE      0x0011A008u  /* Not enough space     */

/* ------------------------------------------------------------------ */
/*  Symbol Table Entry (32 bytes each)                                 */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t address;           /* Symbol address (set at load time)   */
    uint32_t name_offset;       /* Offset into string table            */
    uint16_t ordinal;           /* Stable ordinal                      */
    uint8_t  visibility;        /* OWD_SYM_* constant                  */
    uint8_t  type_hint;         /* Optional type hint (function/data)  */
    uint32_t size;              /* Symbol size in bytes (0 = unknown)  */
    uint32_t checksum;          /* CRC32c of symbol target             */
} owd_symbol_entry_t;

/* ------------------------------------------------------------------ */
/*  Relocation Entry (16 bytes each)                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t offset;            /* Offset within image to patch        */
    uint32_t symbol_index;      /* Index into symbol table             */
    uint8_t  reloc_type;        /* OWD_RELOC_* constant                */
    uint8_t  addend_lo;         /* Low byte of addend                  */
    uint16_t addend_hi;         /* High 16 bits of addend              */
} owd_reloc_entry_t;

/* ------------------------------------------------------------------ */
/*  Dependency Entry (16 bytes each)                                  */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t name_offset;       /* Offset into string table            */
    uint16_t min_version;       /* Minimum required version            */
    uint16_t max_version;       /* Maximum compatible version          */
    uint32_t flags;             /* Dependency-specific flags           */
    uint32_t reserved;          /* Must be zero                        */
} owd_dependency_entry_t;

/* ------------------------------------------------------------------ */
/*  Base Address Constraints                                          */
/*  (Cross-ref: Modular-Bootloader mbl.h memory map)                  */
/*  Libraries loaded above MBL_KERNEL_MAX (0x1000000) for kernel-mode */
/* ------------------------------------------------------------------ */

#define OWD_BASE_KERNEL_MIN     0x00100000u  /* Above bootloader      */
#define OWD_BASE_KERNEL_MAX     0x01000000u  /* Below 16 MiB          */
#define OWD_BASE_USER_MIN       0x10000000u  /* 256 MiB               */
#define OWD_BASE_USER_MAX       0x80000000u  /* 2 GiB                 */
#define OWD_BASE_BOOT_MIN       0x00051000u  /* After MBL_BOOTCONFIG  */
#define OWD_BASE_BOOT_MAX       0x00100000u  /* Below kernel          */

/* ------------------------------------------------------------------ */
/*  .owd Binary Image Header (192 bytes, at offset 0x0)               */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 0x00 */ uint32_t magic;                /* Must be OWD_MAGIC          */
    /* 0x04 */ uint16_t format_version;       /* OWD_FORMAT_VERSION         */
    /* 0x06 */ uint16_t header_size;          /* Must be OWD_HEADER_SIZE    */
    /* 0x08 */ uint32_t image_size;           /* Total image size in bytes  */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0xBF */
    /* 0x10 */ uint64_t base_address;         /* Preferred load address     */
    /* 0x18 */ uint64_t entry_point;          /* Library init entry point   */
    /* 0x20 */ uint64_t entry_point_user;     /* Userspace entry (0 if N/A) */
    /* 0x28 */ uint32_t symbol_count;         /* Number of symbols          */
    /* 0x2C */ uint32_t reloc_count;          /* Number of relocations      */
    /* 0x30 */ uint32_t dependency_count;     /* Number of dependencies     */
    /* 0x34 */ uint32_t string_table_size;    /* Size of string table (B)   */
    /* 0x38 */ uint8_t  lib_type;             /* OWD_LIBTYPE_* constant     */
    /* 0x39 */ uint8_t  target_arch;          /* Architecture (low byte)    */
    /* 0x3A */ uint8_t  target_subarch;       /* Sub-architecture           */
    /* 0x3B */ uint8_t  alignment_log2;       /* Required alignment (2^N)   */
    /* 0x3C */ uint32_t init_flags;           /* Init requirement bitfield  */
    /* 0x40 */ uint64_t timestamp;            /* Build timestamp (UTC secs) */
    /* 0x48 */ uint32_t io_version;           /* Min I/O subsystem version  */
    /* 0x4C */ uint32_t vip_version;          /* Min VIP subsystem version  */
    /* 0x50 */ uint64_t min_memory;           /* Minimum memory required    */
    /* 0x58 */ uint64_t max_memory;           /* Maximum memory usable      */
    /* 0x60 */ uint32_t import_table_offset;  /* Offset to import table     */
    /* 0x64 */ uint32_t symbol_table_offset;  /* Offset to symbol table     */
    /* 0x68 */ uint32_t reloc_table_offset;   /* Offset to reloc table      */
    /* 0x6C */ uint32_t string_table_offset;  /* Offset to string table     */
    /* 0x70 */ uint32_t dependency_offset;    /* Offset to dependency table */
    /* 0x74 */ uint32_t code_section_offset;  /* Offset to code section     */
    /* 0x78 */ uint32_t code_section_size;    /* Size of code section       */
    /* 0x7C */ uint32_t data_section_offset;  /* Offset to data section     */
    /* 0x80 */ uint32_t data_section_size;    /* Size of data section       */
    /* 0x84 */ uint32_t image_checksum;       /* CRC32c of full image       */
    /* 0x88 */ uint32_t padding[14];          /* Pad to 192 bytes, zeros    */
} owd_header_t;

/* ------------------------------------------------------------------ */
/*  Convenience: Validate an .owd header in place                     */
/* ------------------------------------------------------------------ */

static inline bool owd_header_valid(const owd_header_t *h)
{
    if (!h) return false;
    if (h->magic != OWD_MAGIC) return false;
    if (h->header_size != OWD_HEADER_SIZE) return false;
    if (h->symbol_count > OWD_MAX_SYMBOLS) return false;
    if (h->reloc_count > OWD_MAX_RELOC) return false;
    if (h->dependency_count > OWD_MAX_DEPENDS) return false;
    if (h->base_address != 0) {
        if (h->lib_type == OWD_LIBTYPE_KERNEL) {
            if (h->base_address < OWD_BASE_KERNEL_MIN ||
                h->base_address > OWD_BASE_KERNEL_MAX) return false;
        } else if (h->lib_type == OWD_LIBTYPE_USER) {
            if (h->base_address < OWD_BASE_USER_MIN ||
                h->base_address > OWD_BASE_USER_MAX) return false;
        } else if (h->lib_type == OWD_LIBTYPE_BOOT) {
            if (h->base_address < OWD_BASE_BOOT_MIN ||
                h->base_address > OWD_BASE_BOOT_MAX) return false;
        }
    }
    return true;
}

#endif /* OWD_FORMAT_H */
