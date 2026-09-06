/*
 * owk_format.h - OpenWindows Kernel Boot Manifest (.owk) Header Format
 *
 * Defines the structured binary layout for kernel initialization manifests,
 * module load ordering, boot-driver parameters, and hardware limits.
 *
 * Header size: exactly 128 bytes. C99 freestanding.
 */

#ifndef OWK_FORMAT_H
#define OWK_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OWK_MAGIC               0x4F574B31u /* "OWK1" */
#define OWK_HEADER_SIZE         128u
#define OWK_FORMAT_VERSION      0x0001u
#define OWK_MAX_MODULES         64u

typedef struct {
    uint32_t module_type;       /* 0=OWC Driver, 1=OWD Lib, 2=KEXT, 3=Init OWX */
    uint32_t load_priority;     /* Execution order tier (0=immediate) */
    uint64_t image_lba;         /* Storage LBA or ramdisk offset      */
    uint64_t image_size;        /* Byte size of module image          */
    uint32_t name_offset;       /* Offset in manifest string table    */
    uint32_t checksum;          /* Expected CRC32c                    */
} owk_module_entry_t;

typedef struct {
    /* 0x00 */ uint32_t magic;                /* OWK_MAGIC (0x4F574B31)   */
    /* 0x04 */ uint16_t format_version;       /* 0x0001                   */
    /* 0x06 */ uint16_t header_size;          /* 128 bytes                */
    /* 0x08 */ uint32_t manifest_size;        /* Manifest total size      */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0x7F*/
    /* 0x10 */ uint32_t module_count;         /* Number of boot modules   */
    /* 0x14 */ uint32_t module_table_offset;  /* Offset to entries        */
    /* 0x18 */ uint32_t string_table_offset;  /* Offset to string table   */
    /* 0x1C */ uint32_t string_table_size;    /* String table size (B)    */
    /* 0x20 */ uint64_t kernel_pool_size;     /* Pre-allocated static pool*/
    /* 0x28 */ uint64_t early_dma_pool_size;  /* Pre-allocated DMA pool   */
    /* 0x30 */ uint32_t max_cpu_cores;        /* Max active cores allowed */
    /* 0x34 */ uint32_t root_volume_vip_id;   /* Root VIP volume ID       */
    /* 0x38 */ uint32_t panic_behavior_flags; /* Halt, reboot, sentinel   */
    /* 0x3C */ uint32_t reserved_0;           /* Zero                     */
    /* 0x40 */ uint64_t timestamp;            /* Manifest generation time */
    /* 0x48 */ uint32_t manifest_checksum;    /* Full CRC32c              */
    /* 0x4C */ uint32_t padding[13];          /* Pad to 128 bytes         */
} owk_header_t;

static inline bool owk_header_valid(const owk_header_t *h)
{
    if (!h) return false;
    if (h->magic != OWK_MAGIC) return false;
    if (h->header_size != OWK_HEADER_SIZE) return false;
    if (h->module_count > OWK_MAX_MODULES) return false;
    return true;
}

#endif /* OWK_FORMAT_H */
