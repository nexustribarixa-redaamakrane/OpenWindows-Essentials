#ifndef MBL_H
#define MBL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <bancode/bancode_all.h>
#include <sutf/sucs_mode.h>

/* =========================================================================
 * Fixed RAM map
 * ========================================================================= */
#define MBL_FS_BUF         0x00030000u   /* 4 KiB OWFS block staging buffer */
#define MBL_KERNEL_ADDR    0x00200000u   /* kernel load address */
#define MBL_KERNEL_MAX     0x01000000u   /* 16 MiB load cap */
#define MBL_BOOTCONFIG     0x00000510u   /* mbl_boot_config_t (kernel handoff) */

#define MBL_MAGIC_BOOTCFG  0x324C424Du   /* 'MBL2' */

typedef struct {
    uint32_t               magic;        /* MBL_MAGIC_BOOTCFG */
    uint8_t                boot_drive;
    uint8_t                pad[3];
    uint32_t               kernel_size;
    sucs_kernel_boot_config_t sucs_cfg;  /* SuperUnicode kernel boot config */
} mbl_boot_config_t;

/* =========================================================================
 * OpenWindows OWFS on-disk format (read-only bootloader subset)
 *
 * Coexistence strategy (mirrors sucs_types.h / univip_fvip.h): when the
 * authoritative OpenWindows-Storage headers are on the include path they
 * are adopted wholesale and mbl.h defines NOTHING that collides; the
 * spec copies below only kick in for standalone freestanding builds.
 * Values in both paths are byte-identical to owfs_types.h /
 * owfs_superblock.h / owfs_inode.h / owfs_catalog.h / usfs_types.h.
 * ========================================================================= */
#if defined(__has_include)
#  if __has_include(<owfs_types.h>)
#    include <owfs_types.h>
#    include <owfs_superblock.h>
#    include <owfs_inode.h>
#    include <owfs_catalog.h>
#    define MBL_OWFS_ADOPTED 1
#  endif
#  if __has_include(<usfs_types.h>)
#    include <usfs_types.h>
#    define MBL_USFS_ADOPTED 1
#  endif
#endif

#ifndef MBL_OWFS_ADOPTED

#define OWFS_BLOCK_SIZE        0x1000u
#define OWFS_BLOCK_SHIFT       12u
#define OWFS_PARTITION_LBA     131200u   /* GPT partition 2 (== VIP_OWFS_PARTITION_LBA) */
#define OWFS_SUPERBLOCK_BLOCK  16u       /* partition-relative; byte offset 0x10000 */
#define OWFS_INODE_SIZE        0x100u
#define OWFS_INODES_PER_BLOCK  16u
#define OWFS_INODE_TABLE_BLOCKS 256u
#define OWFS_ENTRIES_PER_BLOCK 16u
#define OWFS_NAME_MAX_BYTES    128u
#define OWFS_MAGIC             0x4F574653UL /* 'OWFS' */
#define OWFS_VERSION_MAJOR     1u        /* authoritative spec (owfs_types.h) */
#define OWFS_VERSION_MINOR     2u
#define OWFS_ROOT_INODE        0u
#define OWFS_ENTRY_FILE        0x01u
#define OWFS_ENTRY_CATALOG     0x02u
#define OWFS_ENTRY_DELETED     0x80u
#define OWFS_DIRECT_BLOCKS     10u

/* Volume / inode security flags (identical bit positions to owfs_types.h,
 * usfs_types.h, and the VIP_STORAGE_SEC_* aliases). */
#define OWFS_SEC_ENCRYPTED     (1UL << 0) /* ChaCha20 data-at-rest encryption */
#define OWFS_SEC_READONLY      (1UL << 1) /* writes blocked (MBL is read-only anyway) */
#define OWFS_SEC_HIDDEN        (1UL << 2) /* invisible to non-owner callers */

/* Volume state flags (power-cut protection, owfs_types.h) */
#define OWFS_STATE_CLEAN       0x0000u
#define OWFS_STATE_DIRTY       0x0001u
#define OWFS_STATE_ERROR       0x0002u
#define OWFS_STATE_LOCKED      0x0004u

/* Superblock: block 16 of the partition (byte offset 0x20000).
 * Layout mirrors OpenWindows-Storage/owfs/include/owfs_superblock.h
 * exactly, including the ChaCha20 key slots and per-volume nonce
 * (padded block size 4096 bytes). MBL never uses key material; the
 * fields exist so sizeof/offsets match libowfs byte-for-byte. */
typedef struct __attribute__((packed)) {
    uint32_t magic;                    /* 0x000 */
    uint16_t version_major;            /* 0x004 */
    uint16_t version_minor;            /* 0x006 */
    uint32_t block_size;               /* 0x008 */
    uint32_t total_blocks;             /* 0x00C */
    uint32_t free_blocks;              /* 0x010 */
    uint32_t total_inodes;             /* 0x014 */
    uint32_t free_inodes;              /* 0x018 */
    uint32_t bitmap_start_block;       /* 0x01C */
    uint32_t bitmap_block_count;       /* 0x020 */
    uint32_t inode_table_start;        /* 0x024 */
    uint32_t inode_table_blocks;       /* 0x028 */
    uint32_t data_region_start;        /* 0x02C */
    uint32_t root_inode;               /* 0x030 */
    uint32_t mount_count;              /* 0x034 */
    uint32_t state_flags;              /* 0x038: OWFS_STATE_* */
    uint64_t fletcher64_bitmap;        /* 0x03C */
    uint8_t  volume_label[64];         /* 0x044: SUTF-8 volume label */
    uint32_t checksum;                 /* 0x084: CRC32c (field zeroed) */
    uint32_t security_flags;           /* 0x088: OWFS_SEC_* */
    uint8_t  key_slot_1[256];          /* 0x08C: ChaCha20 key slot 1 */
    uint8_t  key_slot_2[256];          /* 0x18C: ChaCha20 key slot 2 */
    uint8_t  crypto_nonce[12];         /* 0x28C: per-volume ChaCha20 nonce */
    uint8_t  reserved[0x1000 - 0x298]; /* pad to 4096 bytes */
} owfs_superblock_t;

/* Inode: 256 bytes, 16 per block. */
typedef struct __attribute__((packed)) {
    uint32_t inode_number;
    uint8_t  entry_type;
    uint8_t  name_length;
    uint16_t permissions;
    uint16_t flags;
    uint32_t size_bytes;
    uint32_t block_count;
    uint32_t created_timestamp;
    uint32_t modified_timestamp;
    uint32_t parent_inode;
    uint32_t direct_blocks[OWFS_DIRECT_BLOCKS];
    uint32_t indirect_block;
    uint8_t  name[OWFS_NAME_MAX_BYTES];
    uint16_t owner_uid;
    uint16_t owner_gid;
    uint8_t  security_flags;
    uint8_t  reserved[0xFC - 0xCF];
    uint32_t checksum;            /* CRC32c of this inode (field zeroed) */
} owfs_inode_t;

/* Catalog entry: 256 bytes, 16 per block. */
typedef struct __attribute__((packed)) {
    uint32_t inode_number;
    uint8_t  entry_type;
    uint8_t  name_length;
    uint8_t  name[OWFS_NAME_MAX_BYTES];
    uint8_t  reserved[0xFC - 0x86];
    uint32_t checksum;            /* CRC32c of this entry (field zeroed) */
} owfs_catalog_entry_t;

#endif /* !MBL_OWFS_ADOPTED */

/* GPT partition 2 base LBA - not part of libowfs's raw-drive view, so
 * it is always defined here (adopted headers never provide it). */
#ifndef OWFS_PARTITION_LBA
#define OWFS_PARTITION_LBA     131200u   /* == VIP_OWFS_PARTITION_LBA */
#endif

/* Sibling filesystem magic: Universal Secured File System (removable
 * media). Adopted from usfs_types.h when present. */
#ifndef MBL_USFS_ADOPTED
#define USFS_MAGIC             0x55534653UL /* 'USFS' */
#endif

/* =========================================================================
 * Menu entry list
 * ========================================================================= */
#define MBL_MENU_MAX   32
#define MBL_NAME_MAX   128

typedef struct {
    uint32_t inode;
    uint8_t  type;
    char     name[MBL_NAME_MAX];
    uint32_t size;
} mbl_entry_t;

/* =========================================================================
 * UEFI globals (defined in efi_entry.c, accessible from all modules)
 * ========================================================================= */
#include "efi.h"

/* =========================================================================
 * gop.c (GOP framebuffer renderer)
 * ========================================================================= */
void vga_init_gop(void);
void vga_clear(void);
void vga_goto(uint8_t row, uint8_t col);
void vga_putc(char c);
void vga_puts(const char *s);
void vga_write(uint8_t row, uint8_t col, const char *s, uint8_t attr);
void vga_fill(uint8_t row, uint8_t col, uint8_t ch, uint8_t n, uint8_t attr);

/* =========================================================================
 * kbd.c
 * ========================================================================= */
int kbd_poll(void);     /* 0 if no event, else menu action code */
int kbd_wait(void);     /* blocking variant */
void kbd_reboot(void);
uint8_t rtc_get_seconds(void);

/* Menu action codes */
enum {
    MBL_KEY_NONE = 0,
    MBL_KEY_UP, MBL_KEY_DOWN, MBL_KEY_HOME, MBL_KEY_END,
    MBL_KEY_PGUP, MBL_KEY_PGDN, MBL_KEY_ENTER, MBL_KEY_ESC,
    MBL_KEY_REBOOT, MBL_KEY_SHUTDOWN
};

/* =========================================================================
 * disk.c (UEFI Block I/O)
 * ========================================================================= */
int disk_read_sectors(uint32_t lba, uint32_t linear, uint16_t count);
int disk_read_block(uint32_t block, uint32_t linear);

/* =========================================================================
 * bancode_boot.c (BANcode boot diagnostics)
 *
 * All OWFS driver entry points return bancode_t codes drawn from the
 * canonical C+/W+/S+/B+ registry blocks (provisional MBL placements -
 * see <bancode/mbl_bancode.h>). Raised diagnostics are recorded in a
 * fixed-size ring buffer for display and post-mortem.
 * ========================================================================= */
#include <bancode/mbl_bancode.h>

typedef struct {
    bool      fired;     /* always true in returned records */
    uint32_t  slot;      /* Kernel Security Trap slot (0..14, or 15 if n/a) */
    bancode_t trap_cp;   /* resolved trap codepoint, else BANCODE_INVALID_CODEPOINT */
    bancode_t code;      /* raised diagnostic codepoint */
} mbl_diag_t;

void mbl_diag_raise(bancode_t code);
const mbl_diag_t *mbl_diag_last(void);
int mbl_diag_count(void);
const char *mbl_bancode_name(bancode_t code);
void mbl_diag_format(char *buf, bancode_t code); /* buf >= 48 bytes */

/* Classification helpers for driver return values. Direct block-range
 * comparisons so they work with either the vendored bancode_all.h or
 * the upstream generated master header (which lacks W+/C+/S+ helpers). */
static inline bool mbl_code_is_success(bancode_t code) {
    return code >= BANCODE_COMCODE_START && code <= BANCODE_COMCODE_END;
}
static inline bool mbl_code_is_fatal(bancode_t code) {
    return bancode_is_bancode(code);
}
static inline bool mbl_code_is_soft(bancode_t code) {
    return code >= BANCODE_SOFTCODE_START && code <= BANCODE_SOFTCODE_END;
}
static inline bool mbl_code_is_warning(bancode_t code) {
    return code >= BANCODE_WARNCODE_START && code <= BANCODE_WARNCODE_END;
}

/* =========================================================================
 * owfs.c - returns MBL_* BANcode codes (C+ on success)
 * ========================================================================= */
bancode_t owfs_probe(uint8_t drive);
int owfs_enumerate(mbl_entry_t *entries, int max); /* entry count, <0 on fault */
bancode_t owfs_load_file(uint32_t inode, uint32_t dest, uint32_t *size_out);
uint8_t owfs_boot_drive(void);

/* =========================================================================
 * menu.c
 * ========================================================================= */
int menu_run(const mbl_entry_t *entries, int count, int timeout_secs);

/* =========================================================================
 * main.c
 * ========================================================================= */
void kmain(void);

#endif /* MBL_H */
