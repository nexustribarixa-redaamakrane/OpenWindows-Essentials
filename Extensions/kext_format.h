/*
 * kext_format.h - OpenWindows Kernel Extension (.kext) Binary Header Format
 *
 * Defines the binary interface for modular kernel extensions (kextensions)
 * dynamically attached to kernel subsystems (networking, storage, VMM, audio).
 *
 * Header size: exactly 192 bytes, 8-byte naturally aligned.
 * C99 freestanding - zero dynamic allocation.
 */

#ifndef KEXT_FORMAT_H
#define KEXT_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KEXT_MAGIC              0x4B455854u /* "KEXT" in little-endian */
#define KEXT_HEADER_SIZE        192u        /* Fixed header size (bytes) */
#define KEXT_FORMAT_VERSION     0x0001u
#define KEXT_MAX_HOOKS          32u

/* Subsystem Target Domains */
#define KEXT_DOMAIN_GENERIC     0x00u
#define KEXT_DOMAIN_STORAGE     0x01u       /* VIP / OWFS / USFS stack   */
#define KEXT_DOMAIN_NETWORK     0x02u       /* Netwerk / packet filters  */
#define KEXT_DOMAIN_MEMORY      0x03u       /* VMM / page pool hook      */
#define KEXT_DOMAIN_SECURITY    0x04u       /* BANcode / sentinel hook   */
#define KEXT_DOMAIN_GRAPHICS    0x05u       /* Cairo / framebuffer hook  */

typedef struct {
    uint32_t hook_id;           /* Kernel hook identifier            */
    uint32_t flags;             /* Pre/post execution flags          */
    uint64_t handler_offset;    /* Offset to callback function       */
    uint32_t priority;          /* Execution priority (0=highest)    */
    uint32_t reserved;          /* Must be zero                      */
} kext_hook_entry_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* KEXT_MAGIC (0x4B455854)   */
    /* 0x04 */ uint16_t format_version;       /* KEXT_FORMAT_VERSION       */
    /* 0x06 */ uint16_t header_size;          /* KEXT_HEADER_SIZE (192)    */
    /* 0x08 */ uint32_t image_size;           /* Full image byte size      */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0xBF*/
    /* 0x10 */ uint64_t init_entry;           /* kext_init() offset        */
    /* 0x18 */ uint64_t fini_entry;           /* kext_fini() offset        */
    /* 0x20 */ uint8_t  domain;               /* KEXT_DOMAIN_* constant    */
    /* 0x21 */ uint8_t  target_arch;          /* Target CPU architecture   */
    /* 0x22 */ uint16_t flags;                /* Extension flags           */
    /* 0x24 */ uint32_t hook_count;           /* Number of hook entries    */
    /* 0x28 */ uint32_t hook_table_offset;    /* Offset to hook table      */
    /* 0x2C */ uint32_t string_table_offset;  /* Offset to string table    */
    /* 0x30 */ uint32_t string_table_size;    /* String table byte size    */
    /* 0x34 */ uint32_t required_kernel_ver;  /* Minimum kernel version    */
    /* 0x38 */ uint64_t timestamp;            /* Build UTC timestamp       */
    /* 0x40 */ uint32_t sentinel_bancode;     /* Fault sentinel trigger    */
    /* 0x44 */ uint32_t sentinel_trap_slot;   /* Trap slot (0..14)         */
    /* 0x48 */ uint32_t code_section_offset;  /* Offset to code            */
    /* 0x4C */ uint32_t code_section_size;    /* Byte size of code         */
    /* 0x50 */ uint32_t data_section_offset;  /* Offset to data            */
    /* 0x54 */ uint32_t data_section_size;    /* Byte size of data         */
    /* 0x58 */ uint32_t image_checksum;       /* Full CRC32c checksum      */
    /* 0x5C */ uint32_t padding[25];          /* Zero pad to 192 bytes     */
} kext_header_t;

static inline bool kext_header_valid(const kext_header_t *h)
{
    if (!h) return false;
    if (h->magic != KEXT_MAGIC) return false;
    if (h->header_size != KEXT_HEADER_SIZE) return false;
    if (h->hook_count > KEXT_MAX_HOOKS) return false;
    return true;
}

#endif /* KEXT_FORMAT_H */
