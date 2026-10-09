/*
 * kmod_format.h - OpenWindows Kernel Micro-Module (.kmod) Header Format
 *
 * Defines lightweight, relocatable bare-metal micro-modules loaded directly
 * into ring 0 for early boot, hardware quirks, and bus bridges.
 *
 * Header size: exactly 192 bytes. C99 freestanding.
 */

#ifndef KMOD_FORMAT_H
#define KMOD_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KMOD_MAGIC              0x4B4D4F44u /* "KMOD" */
#define KMOD_HEADER_SIZE        192u
#define KMOD_FORMAT_VERSION     0x0001u
#define KMOD_MAX_RELOCS         128u

typedef struct {
    uint32_t offset;            /* In-module offset to patch         */
    uint16_t type;              /* Relocation type                   */
    uint16_t symbol_idx;        /* Target symbol index               */
    int64_t  addend;            /* Addend value                      */
} kmod_reloc_entry_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* KMOD_MAGIC (0x4B4D4F44)   */
    /* 0x04 */ uint16_t format_version;       /* 0x0001                    */
    /* 0x06 */ uint16_t header_size;          /* 192 bytes                 */
    /* 0x08 */ uint32_t module_size;          /* Total size in bytes       */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0xBF*/
    /* 0x10 */ uint64_t init_entry;           /* Module init function      */
    /* 0x18 */ uint64_t halt_entry;           /* Module shutdown handler   */
    /* 0x20 */ uint32_t reloc_count;          /* Number of relocations     */
    /* 0x24 */ uint32_t reloc_table_offset;   /* Offset to relocations     */
    /* 0x28 */ uint32_t code_offset;          /* Executable code offset    */
    /* 0x2C */ uint32_t code_size;            /* Code size in bytes        */
    /* 0x30 */ uint32_t data_offset;          /* Data section offset       */
    /* 0x34 */ uint32_t data_size;            /* Data size in bytes        */
    /* 0x38 */ uint32_t bss_size;             /* BSS size (unmapped zero)  */
    /* 0x3C */ uint32_t target_arch;          /* Target CPU arch           */
    /* 0x40 */ uint64_t timestamp;            /* Build timestamp           */
    /* 0x48 */ uint32_t module_checksum;      /* Full image CRC32c         */
    /* 0x4C */ uint32_t reserved_flags;       /* Reserved flags            */
    /* 0x50 */ uint32_t padding[28];          /* Pad to 192 bytes          */
} kmod_header_t;

static inline bool kmod_header_valid(const kmod_header_t *h)
{
    if (!h) return false;
    if (h->magic != KMOD_MAGIC) return false;
    if (h->header_size != KMOD_HEADER_SIZE) return false;
    if (h->reloc_count > KMOD_MAX_RELOCS) return false;
    return true;
}

#endif /* KMOD_FORMAT_H */
