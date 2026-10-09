/* univip_fvip.h - Volume Indexing Protocols (VIPs) Framework
 *
 * Defines the foundational VIP abstraction layer, FVIP (File Volume Indexing
 * Protocol) data structures, and UniVIP (Universal Volume Indexing Protocol)
 * concrete implementation using a 44-bit sparse hex trie.
 *
 * OpenWindows ecosystem compatibility (header-only, zero external deps):
 *   - BANcode: every return value is a uint32_t codepoint inside the
 *     canonical C+/W+/S+/B+ registry blocks. Classification helpers and
 *     Kernel Security Trap mapping mirror <bancode/bancode_all.h>.
 *     Concrete slot values below are PROVISIONAL placements within the
 *     correct blocks/subcategories - no slots are claimed in the BANcode
 *     registries yet; names stay stable when official slots are assigned.
 *   - SuperUnicode: paths and volume labels are SUTF-8 byte streams mapped
 *     to SuperUnicode 31-bit codepoints. Guarded SUCS constants coexist
 *     with <sucs_types.h>; the bundled codec mirrors libsutf semantics
 *     (overlong rejection, sentinel & trap-range exclusion).
 *   - OpenWindows-storage: storage-block addressing aligned with
 *     OWFS/USFS (0x1000 blocks, 0x100 entries), entry-type/security-flag
 *     conversion for both filesystems, HTL-compatible absolute addressing.
 *   - Modular-Bootloader: base sectors are absolute 512-byte LBAs matching
 *     GPT / UEFI Block I/O units, honoring the MBL reserved drive region.
 *
 * Architecture: 44-Bit Sparse Radix / Hex Trie Indexing
 * Standard: C99 (-std=c99 -Wall -Wextra -pedantic)
 * Environment: Freestanding (-ffreestanding -nostdlib)
 * Static linking: Compiles into libvip.a
 */
#ifndef UNIVIP_FVIP_H
#define UNIVIP_FVIP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * SuperUnicode (SUCS) Constants
 *
 * Guarded so identical constants can safely coexist with
 * superunicode/sutf/include/sucs_types.h in a single translation unit.
 * ========================================================================= */

#ifndef SUCS_INVALID_CODEPOINT
#define SUCS_INVALID_CODEPOINT  0x7FFFFFFFUL
#endif
#ifndef SUCS_MAX_CODEPOINT
#define SUCS_MAX_CODEPOINT      0x7FFFFFFFUL
#endif
#ifndef SUCS_TRAP_RANGE_MIN
#define SUCS_TRAP_RANGE_MIN     0x7FFFFFF0UL
#endif
#ifndef SUCS_TRAP_RANGE_MAX
#define SUCS_TRAP_RANGE_MAX     0x7FFFFFFEUL
#endif

/* =========================================================================
 * BANcode Diagnostic & Status Codes (canonical registry alignment)
 *
 * Return type for all public APIs. 32-bit hex codes drawn from the four
 * official BANcode registry blocks:
 *   B+ BANcode   0x0011A000-0x0011A7FF  fatal corruption -> kernel halt
 *   W+ WARNcode  0x0011A800-0x0011ABFF  non-fatal telemetry (unused by VIP)
 *   C+ COMcode   0x0011AC00-0x0011ADFF  success / completion reports
 *   S+ SOFTcode  0x0011AE00-0x0011AEFF  recoverable soft faults
 * ========================================================================= */

/* All VIP APIs return plain uint32_t holding bancode_t codepoint values.
 * <bancode/bancode_all.h> typedefs bancode_t WITHOUT an include guard, so
 * re-declaring that typedef here would break single-TU coexistence in
 * either include order under strict C99 (-pedantic). Codepoints returned
 * by this library assign cleanly to bancode_t variables wherever the
 * BANcode framework headers are present. */

/* Block boundaries (identical values to BANCODE_*_START/END) */
#define VIP_BANCODE_BLOCK_START    0x0011A000U
#define VIP_BANCODE_BLOCK_END      0x0011A7FFU
#define VIP_WARNCODE_BLOCK_START   0x0011A800U
#define VIP_WARNCODE_BLOCK_END     0x0011ABFFU
#define VIP_COMCODE_BLOCK_START    0x0011AC00U
#define VIP_COMCODE_BLOCK_END      0x0011ADFFU
#define VIP_SOFTCODE_BLOCK_START   0x0011AE00U
#define VIP_SOFTCODE_BLOCK_END     0x0011AEFFU

/* Trap dispatch geometry (15 Kernel Security Trap slots x 128 codes) */
#define VIP_TRAP_SLOT_COUNT        15U
#define VIP_BANCODES_PER_TRAP      128U

static inline bool vip_code_is_fatal(uint32_t code) {
    return (code >= VIP_BANCODE_BLOCK_START) && (code <= VIP_BANCODE_BLOCK_END);
}

static inline bool vip_code_is_warning(uint32_t code) {
    return (code >= VIP_WARNCODE_BLOCK_START) && (code <= VIP_WARNCODE_BLOCK_END);
}

static inline bool vip_code_is_success(uint32_t code) {
    return (code >= VIP_COMCODE_BLOCK_START) && (code <= VIP_COMCODE_BLOCK_END);
}

static inline bool vip_code_is_soft(uint32_t code) {
    return (code >= VIP_SOFTCODE_BLOCK_START) && (code <= VIP_SOFTCODE_BLOCK_END);
}

/* Resolves the Kernel Security Trap codepoint (0x7FFFFFF0-0x7FFFFFFE)
 * governing a fatal B+ code for damage-control dispatch. Returns
 * SUCS_INVALID_CODEPOINT for non-fatal codes or the unmapped final
 * cluster (0x0011A780-0x0011A7FF). Mirrors bancode_to_trap(). */
static inline uint32_t vip_bancode_to_trap(uint32_t code) {
    uint32_t slot;
    if (!vip_code_is_fatal(code)) {
        return (uint32_t)SUCS_INVALID_CODEPOINT;
    }
    slot = (uint32_t)(code - VIP_BANCODE_BLOCK_START) / VIP_BANCODES_PER_TRAP;
    if (slot >= VIP_TRAP_SLOT_COUNT) {
        return (uint32_t)SUCS_INVALID_CODEPOINT;
    }
    return (uint32_t)(SUCS_TRAP_RANGE_MIN + (uint32_t)slot);
}

/* -- Success codes (C+, COMcode block: storage & boot success reports,
 *    provisional placement at 0x0011AD00+) -------------------------------- */
#define VIP_OK                          0x0011AD00U
#define VIP_INIT_OK                     0x0011AD01U
#define VIP_REGISTER_OK                 0x0011AD02U
#define VIP_RESOLVE_OK                  0x0011AD03U
#define VIP_INSERT_OK                   0x0011AD04U
#define VIP_LOOKUP_OK                   0x0011AD05U
#define VIP_REMOVE_OK                   0x0011AD06U
#define VIP_INTEGRITY_OK                0x0011AD07U

/* -- Recoverable soft faults (S+, SOFTcode block: boot & protocol soft
 *    errors, provisional placement at 0x0011AEE0+) ------------------------ */
#define VIP_ERR_INVALID_PARAM           0x0011AEE0U
#define VIP_ERR_NULL_POINTER            0x0011AEE1U
#define VIP_ERR_NOT_FOUND               0x0011AEE2U
#define VIP_ERR_ALREADY_EXISTS          0x0011AEE3U
#define VIP_ERR_TABLE_FULL              0x0011AEE4U
#define VIP_ERR_VOLUME_NOT_REGISTERED   0x0011AEE5U
#define VIP_ERR_VOLUME_LIMIT            0x0011AEE6U
#define VIP_ERR_LABEL_TOO_LONG          0x0011AEE7U
#define VIP_ERR_PATH_TOO_LONG           0x0011AEE8U
#define VIP_ERR_NOT_INITIALIZED         0x0011AEE9U
#define VIP_ERR_SYSTEM_NOT_READY        0x0011AEEAU
#define VIP_ERR_INTEGRITY_FAIL          0x0011AEEBU
#define VIP_ERR_INVALID_SUTF8           0x0011AEECU

/* -- Fatal corruption (B+, provisional tail run of the storage-integrity
 *    neighborhood; re-home when dedicated VIP slots are registered) ------- */
#define VIP_BAN_TABLE_CORRUPT           0x0011A3E0U
#define VIP_BAN_NODE_CORRUPT            0x0011A3E1U
#define VIP_BAN_INDEX_OVERRUN           0x0011A3E2U
#define VIP_BAN_POOL_EXHAUSTED          0x0011A3E3U
#define VIP_BAN_TRIE_DEPTH_EXCEEDED     0x0011A3E4U
#define VIP_BAN_SYSTEM_NOT_INITIALIZED  0x0011A3E5U
#define VIP_BAN_ENTRY_CORRUPT           0x0011A3E6U

/* =========================================================================
 * Storage & Boot Layout Constants
 *
 * Canonical OWFS/USFS/MBL values aliased under VIP-prefixed names.
 * owfs_types.h, usfs_types.h, and mbl.h define these WITHOUT include
 * guards, so this header never redefines their names - it adopts them
 * when present and falls back to the spec values otherwise. This keeps
 * single-TU inclusion safe in any include order.
 * ========================================================================= */

/* 4096-byte storage blocks (OWFS_BLOCK_SIZE / USFS_BLOCK_SIZE) */
#if defined(OWFS_BLOCK_SIZE)
#define VIP_STORAGE_BLOCK_SIZE      OWFS_BLOCK_SIZE
#elif defined(USFS_BLOCK_SIZE)
#define VIP_STORAGE_BLOCK_SIZE      USFS_BLOCK_SIZE
#else
#define VIP_STORAGE_BLOCK_SIZE      0x1000U
#endif

/* On-disk entry type bits (shared OWFS/USFS layout) */
#ifdef OWFS_ENTRY_FILE
#define VIP_STORAGE_ENTRY_FILE      OWFS_ENTRY_FILE
#elif defined(USFS_ENTRY_FILE)
#define VIP_STORAGE_ENTRY_FILE      USFS_ENTRY_FILE
#else
#define VIP_STORAGE_ENTRY_FILE      0x01U
#endif

#ifdef OWFS_ENTRY_CATALOG
#define VIP_STORAGE_ENTRY_CATALOG   OWFS_ENTRY_CATALOG
#elif defined(USFS_ENTRY_CATALOG)
#define VIP_STORAGE_ENTRY_CATALOG   USFS_ENTRY_CATALOG
#else
#define VIP_STORAGE_ENTRY_CATALOG   0x02U
#endif

#ifdef OWFS_ENTRY_DELETED
#define VIP_STORAGE_ENTRY_DELETED   OWFS_ENTRY_DELETED
#elif defined(USFS_ENTRY_DELETED)
#define VIP_STORAGE_ENTRY_DELETED   USFS_ENTRY_DELETED
#else
#define VIP_STORAGE_ENTRY_DELETED   0x80U
#endif

/* Volume/inode security flag bits (identical positions in OWFS & USFS) */
#ifdef OWFS_SEC_ENCRYPTED
#define VIP_STORAGE_SEC_ENCRYPTED   OWFS_SEC_ENCRYPTED
#elif defined(USFS_SEC_ENCRYPTED)
#define VIP_STORAGE_SEC_ENCRYPTED   USFS_SEC_ENCRYPTED
#else
#define VIP_STORAGE_SEC_ENCRYPTED   (1U << 0)
#endif

#ifdef OWFS_SEC_READONLY
#define VIP_STORAGE_SEC_READONLY    OWFS_SEC_READONLY
#elif defined(USFS_SEC_READONLY)
#define VIP_STORAGE_SEC_READONLY    USFS_SEC_READONLY
#else
#define VIP_STORAGE_SEC_READONLY    (1U << 1)
#endif

#ifdef OWFS_SEC_HIDDEN
#define VIP_STORAGE_SEC_HIDDEN      OWFS_SEC_HIDDEN
#elif defined(USFS_SEC_HIDDEN)
#define VIP_STORAGE_SEC_HIDDEN      USFS_SEC_HIDDEN
#else
#define VIP_STORAGE_SEC_HIDDEN      (1U << 2)
#endif

/* MBL GPT data partition 2 (mbl.h / owfs_mkfs.py layout) */
#ifdef OWFS_PARTITION_LBA
#define VIP_OWFS_PARTITION_LBA      OWFS_PARTITION_LBA
#else
#define VIP_OWFS_PARTITION_LBA      131200U
#endif

/* VIP addressing model:
 *   - Volume base sectors are ABSOLUTE 512-byte LBAs (GPT / UEFI Block I/O
 *     units used by the Modular Bootloader disk reader).
 *   - FVIP entry offsets are VOLUME-RELATIVE BYTE OFFSETS (usable directly
 *     for OWFS/USFS block math: block = offset >> OWFS_BLOCK_SHIFT). */
#define VIP_SECTOR_SIZE             512U
#define VIP_MBL_RESERVED_BASE_LBA   0U       /* drive bytes 0x0000-0xFFFF */
#define VIP_MBL_RESERVED_LBA_COUNT  128U     /* reserved for MBL boot stages */
#define VIP_LBAS_PER_STORAGE_BLOCK  (VIP_STORAGE_BLOCK_SIZE / VIP_SECTOR_SIZE) /* 8 */

/* =========================================================================
 * Configuration Constants
 * ========================================================================= */

#define VIP_MAX_VOLUMES         16U
#define VIP_LABEL_MAX           64U
#define FVIP_MAX_ENTRIES        128U
#define FVIP_PATH_MAX           128U
#define UNIVIP_MAX_NODES        512U
#define UNIVIP_NULL_NODE        0xFFFFFFFFU
#define UNIVIP_NULL_ENTRY       0xFFFFFFFFU
#define UNIVIP_KEY_BITS         44U
#define UNIVIP_NIBBLE_COUNT     11U
#define UNIVIP_CHILDREN_PER_NODE 16U

/* =========================================================================
 * File Attribute Flags
 *
 * FVIP-side flags convert losslessly to/from OWFS/USFS entry types and
 * security flags via fvip_flags_from_storage_entry() /
 * fvip_flags_to_storage_entry().
 * ========================================================================= */

#define FVIP_FLAG_FILE          0x0001U
#define FVIP_FLAG_CATALOG       0x0002U
#define FVIP_FLAG_HIDDEN        0x0004U
#define FVIP_FLAG_READONLY      0x0008U
#define FVIP_FLAG_ENCRYPTED     0x0010U
#define FVIP_FLAG_SYSTEM        0x0020U
#define FVIP_FLAG_DELETED       0x8000U

/* =========================================================================
 * UniVIP (Universal Volume Indexing Protocol) - Hex Trie Nodes
 *
 * 44-bit sparse radix tree: key decomposed into 11 nibbles (MSB first),
 * each node has up to 16 children indexed by nibble value 0x0-0xF.
 * Sparse allocation via 16-bit child bitmap. Zero dynamic allocation.
 * ========================================================================= */

/* Single hex trie node */
typedef struct {
    uint16_t child_bitmap;
    uint32_t entry_index;
    uint32_t children[UNIVIP_CHILDREN_PER_NODE];
} univip_node_t;

/* Hex trie with pre-allocated node pool (slab-style) */
typedef struct {
    univip_node_t nodes[UNIVIP_MAX_NODES];
    uint32_t free_stack[UNIVIP_MAX_NODES];
    uint32_t free_top;
    uint32_t root;
    uint32_t node_count;
    uint32_t entry_count;
} univip_tree_t;

/* =========================================================================
 * VIP General Infrastructure - Volume Registry
 * ========================================================================= */

/* Single volume registration entry.
 * base_sector is the volume's absolute start address in 512-byte LBAs
 * (e.g. OWFS_PARTITION_LBA for the MBL GPT data partition, or
 * VIP_MBL_RESERVED_LBA_COUNT for a raw libowfs drive layout). */
typedef struct {
    uint32_t volume_id;
    uint64_t base_sector;
    char     label[VIP_LABEL_MAX];
    bool     registered;
} vip_volume_entry_t;

/* Global volume registry (static pool, zero dynamic allocation) */
typedef struct {
    vip_volume_entry_t entries[VIP_MAX_VOLUMES];
    uint32_t count;
    bool     initialized;
} vip_registry_t;

/* =========================================================================
 * FVIP (File Volume Indexing Protocol) - File-Level Structures
 * ========================================================================= */

/* Individual file index entry.
 * path            : SUTF-8 byte stream (validated at insert time)
 * sector_offset   : volume-relative BYTE offset (see addressing model)
 * codepoint_metadata : number of SUCS codepoints in path (verified by
 *                      integrity checks) */
typedef struct {
    char     path[FVIP_PATH_MAX];
    uint64_t sector_offset;
    uint32_t flags;
    uint32_t codepoint_metadata;
    uint64_t created_epoch;
    uint64_t modified_epoch;
    uint64_t path_hash;
    bool     occupied;
} fvip_entry_t;

/* Pre-allocated FVIP table with integrated UniVIP trie.
 * The trie is embedded inline via the reserved buffer sized to
 * exactly fit univip_tree_t. Cast in implementation code. */
typedef struct {
    fvip_entry_t entries[FVIP_MAX_ENTRIES];
    uint32_t     count;
    uint32_t     volume_id;
    bool         initialized;
    uint8_t      _trie_reserved[sizeof(univip_tree_t)];
} fvip_table_t;

/* =========================================================================
 * Public API - Initialization
 * ========================================================================= */

/* Initialize the global VIP system (volume registry). Must be called
 * before any other VIP function. Returns VIP_INIT_OK on success. */
uint32_t univip_init_system(void);

/* Initialize an FVIP volume index table for the given volume_id.
 * The UniVIP trie within the table is also initialized. */
uint32_t fvip_init_volume_index(uint32_t volume_id, fvip_table_t *out_table);

/* =========================================================================
 * Public API - UniVIP Volume-Level Navigation
 * ========================================================================= */

/* Register a volume: associates volume_id with an absolute base LBA and
 * an SUTF-8 label. The volume_id must not already be registered. */
uint32_t univip_register_volume(uint32_t volume_id, uint64_t base_sector,
                                 const char *vol_label);

/* Resolve a registered volume: returns the absolute base LBA (512-byte
 * sectors) for volume_id. */
uint32_t univip_resolve_volume(uint32_t volume_id, uint64_t *out_base_sector);

/* =========================================================================
 * Public API - FVIP File-Level Indexing & Fast Path Query
 * ========================================================================= */

/* Insert a file entry into the FVIP table. The SUTF-8 path is validated,
 * hashed and indexed in the UniVIP trie for O(11) lookup. Flags accept
 * FVIP_FLAG_* values (convert from OWFS/USFS via the helpers below).
 * Returns VIP_INSERT_OK on success. */
uint32_t fvip_insert_entry(fvip_table_t *table, const char *path,
                            uint64_t sector_offset, uint32_t flags);

/* Lookup a file entry by path. Uses UniVIP trie for fast resolution.
 * On success, out_entry is populated and VIP_LOOKUP_OK is returned. */
uint32_t fvip_lookup_entry(const fvip_table_t *table, const char *path,
                            fvip_entry_t *out_entry);

/* Remove a file entry by path. Marks the entry as unoccupied and
 * removes it from the UniVIP trie. Returns VIP_REMOVE_OK on success. */
uint32_t fvip_remove_entry(fvip_table_t *table, const char *path);

/* =========================================================================
 * Public API - Absolute Addressing (OpenWindows-storage / MBL integration)
 * ========================================================================= */

/* Resolve an occupied entry to its ABSOLUTE byte offset on the drive:
 * (volume_base_lba * 512) + entry->sector_offset. Requires the table's
 * volume to be registered. */
uint32_t fvip_entry_absolute_byte(const fvip_table_t *table,
                                   const fvip_entry_t *entry,
                                   uint64_t *out_byte);

/* Resolve an occupied entry to its ABSOLUTE 512-byte LBA (the unit used
 * by MBL UEFI Block I/O reads): volume_base_lba + offset / 512. */
uint32_t fvip_entry_absolute_lba(const fvip_table_t *table,
                                  const fvip_entry_t *entry,
                                  uint64_t *out_lba);

/* =========================================================================
 * Public API - Storage Flag Conversion (OWFS & USFS)
 * ========================================================================= */

/* Build FVIP flags from an on-disk entry type byte plus security flags.
 * Entry types use the shared OWFS/USFS layout (FILE=0x01, CATALOG=0x02,
 * DELETED=0x80); security flags use the shared bit positions
 * (ENCRYPTED=bit0, READONLY=bit1, HIDDEN=bit2). FVIP_FLAG_SYSTEM has no
 * on-disk equivalent and is never produced here. */
uint32_t fvip_flags_from_storage_entry(uint8_t entry_type, uint32_t sec_flags);

/* Inverse conversion: emits the entry type byte and security flags for
 * OWFS or USFS on-disk structures. If neither FILE nor CATALOG is set,
 * the entry type defaults to FILE. FVIP_FLAG_SYSTEM is dropped. */
void fvip_flags_to_storage_entry(uint32_t fvip_flags,
                                 uint8_t *out_entry_type,
                                 uint32_t *out_sec_flags);

/* =========================================================================
 * Public API - SuperUnicode / SUTF-8 Compatibility Codec
 *
 * Freestanding reimplementation matching libsutf transport rules:
 * 1-6 byte streams, overlong rejection, trap-range/sentinel exclusion.
 * Prefix avoids symbol clashes when linked alongside libsutf.a.
 * ========================================================================= */

/* Byte length the codepoint occupies in a canonical SUTF-8 stream.
 * Returns 0 for invalid codepoints (mirrors sutf8_codepoint_length). */
size_t fvip_sutf8_codepoint_length(uint32_t cp);

/* Decode one SUTF-8 codepoint. Returns bytes consumed, or 0 on error
 * with *out_cp set to SUCS_INVALID_CODEPOINT (mirrors sutf8_decode_char). */
size_t fvip_sutf8_decode_char(const uint8_t *in_buf, size_t buf_size,
                              uint32_t *out_cp);

/* Validate that a NUL-terminated string is a well-formed SUTF-8 stream
 * (used for paths and volume labels). Empty strings are valid. */
bool fvip_str_is_sutf8(const char *s);

/* Count the SUCS codepoints in a NUL-terminated SUTF-8 string.
 * Returns VIP_OK, or VIP_ERR_INVALID_SUTF8 on malformed input. */
uint32_t fvip_str_codepoint_count(const char *s, size_t *out_count);

/* =========================================================================
 * Public API - Diagnostics
 * ========================================================================= */

/* Verify structural integrity of the FVIP table:
 *  - Entry count matches occupied slots
 *  - All occupied entries have correct path hashes
 *  - Stored codepoint_metadata matches the recomputed SUTF-8 count
 *  - UniVIP trie terminal count matches entry count
 * Returns VIP_INTEGRITY_OK or a specific B+/S+ diagnostic. */
uint32_t fvip_verify_integrity(const fvip_table_t *table);

#ifdef __cplusplus
}
#endif

#endif /* UNIVIP_FVIP_H */
