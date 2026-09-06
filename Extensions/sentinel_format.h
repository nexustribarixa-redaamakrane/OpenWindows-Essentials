/*
 * sentinel_format.h - Sentinel Recovery Directive (.sentinel) Binary Format
 *
 * Defines the binary layout for crash-state recovery scripts executed
 * by the Damage Control Sentinel after a system panic triggered by a
 * BANcode fault. These directives describe repair operations, state
 * checksums for corruption validation, and fallback triggers.
 *
 * Alignment: All fields are naturally aligned. The header occupies
 * exactly 128 bytes. Directive blocks are variable-length, each
 * preceded by a 16-byte block header.
 *
 * C99 freestanding - no dynamic allocation.
 */

#ifndef SENTINEL_FORMAT_H
#define SENTINEL_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  Magic & Versioning                                                 */
/* ------------------------------------------------------------------ */

#define SENTINEL_MAGIC          0x534E544C  /* "SNTL" in little-endian */
#define SENTINEL_HEADER_SIZE    128u        /* Fixed header size       */
#define SENTINEL_FORMAT_VERSION 0x0001u     /* Major.Minor = 1.0       */
#define SENTINEL_MAX_DIRECTIVES 64u         /* Max repair opcodes      */
#define SENTINEL_MAX_CHECKSUMS  32u         /* Max state checksums     */
#define SENTINEL_MAX_TRIGGERS   16u         /* Max fallback triggers   */

/* ------------------------------------------------------------------ */
/*  Directive Block Types (Repair Opcodes)                             */
/* ------------------------------------------------------------------ */

#define SENTINEL_OP_NOOP            0x00u  /* No operation (padding)   */
#define SENTINEL_OP_RESTORE_SHADOW  0x01u  /* Restore shadow file      */
                                            /* table from backup        */
#define SENTINEL_OP_ISOLATE_BUS     0x02u  /* Isolate a faulted        */
                                            /* hardware bus             */
#define SENTINEL_OP_LOG_STATE       0x03u  /* Write diagnostic state   */
                                            /* to boot storage          */
#define SENTINEL_OP_RESET_DEVICE    0x04u  /* Reset a specific device  */
#define SENTINEL_OP_REBUILD_FVIP    0x05u  /* Rebuild FVIP table from  */
                                            /* UniVIP trie              */
#define SENTINEL_OP_FLUSH_CACHE     0x06u  /* Flush and invalidate    */
                                            /* specified cache range    */
#define SENTINEL_OP_CHECKPOINT      0x07u  /* Write recovery           */
                                            /* checkpoint               */
#define SENTINEL_OP_ROLLBACK        0x08u  /* Rollback to checkpoint   */
#define SENTINEL_OP_SHUTDOWN_FAIL   0x09u  /* Controlled shutdown on   */
                                            /* unrecoverable failure    */
#define SENTINEL_OP_RELOCATE        0x0Au  /* Relocate data from bad   */
                                            /* sectors to spare area    */
#define SENTINEL_OP_RAISE_BANCODE   0x0Bu  /* Raise a BANcode to       */
                                            /* trigger secondary handler*/
#define SENTINEL_OP_WAIT_EVENT      0x0Cu  /* Block until specified    */
                                            /* hardware event           */
#define SENTINEL_OP_INVOKE_DRIVER   0x0Du  /* Call into a specific     */
                                            /* .owc driver handler      */
#define SENTINEL_OP_INVOKE_DLL      0x0Eu  /* Call into a specific     */
                                            /* .owd library handler     */

/* ------------------------------------------------------------------ */
/*  Condition Flags (on directive blocks)                              */
/* ------------------------------------------------------------------ */

#define SENTINEL_COND_NONE          0x0000u
#define SENTINEL_COND_CHECKSUM_OK   0x0001u  /* Only if checksum match */
#define SENTINEL_COND_CHECKSUM_BAD  0x0002u  /* Only if checksum fail  */
#define SENTINEL_COND_RETRY         0x0004u  /* Retriable directive    */
#define SENTINEL_COND_CRITICAL      0x0008u  /* If this fails, abort   */
#define SENTINEL_COND_BOOT_ONLY     0x0010u  /* Only during boot       */
#define SENTINEL_COND_HOT_PLUG      0x0020u  /* Can run while live     */

/* ------------------------------------------------------------------ */
/*  Checksum Types                                                    */
/*  (Cross-ref: OpenWindows-Storage owfs_superblock_t CRC/Fletcher)   */
/* ------------------------------------------------------------------ */

#define SENTINEL_CKSUM_NONE         0x00u
#define SENTINEL_CKSUM_CRC32C       0x01u
#define SENTINEL_CKSUM_FLETCHER16   0x02u
#define SENTINEL_CKSUM_FLETCHER32   0x03u
#define SENTINEL_CKSUM_XOR16        0x04u

/* ------------------------------------------------------------------ */
/*  BANcode Integration                                               */
/*  (Cross-ref: BANcode bancode_all.h B+ range 0x0011A000-0x0011A7FF) */
/*  Sentinel directives trigger on B+ BANcodes. The trigger_bancode    */
/*  field in the header specifies which B+ code initiates this script */
/* ------------------------------------------------------------------ */

#define SENTINEL_BANCODE_RANGE_LO   0x0011A000u
#define SENTINEL_BANCODE_RANGE_HI   0x0011A7FFu
#define SENTINEL_TRAP_BASE          0x7FFFFFF0u
#define SENTINEL_TRAP_END           0x7FFFFFFEu

/* ------------------------------------------------------------------ */
/*  VIP Storage Constants                                             */
/*  (Cross-ref: vip/univip_fvip.h)                                    */
/* ------------------------------------------------------------------ */

#define SENTINEL_VIP_SECTOR_SIZE    512u
#define SENTINEL_VIP_MBL_RESERVED   128u     /* LBA count reserved     */
#define SENTINEL_VIP_OWFS_LBA       131200u  /* OWFS partition start    */

/* ------------------------------------------------------------------ */
/*  Status Codes — BANcode mapped                                      */
/*  B+ (0x0011A000-0x0011A77F): Fatal sentinel faults                  */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal warnings                    */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
/* ------------------------------------------------------------------ */

typedef uint32_t sentinel_status_t;

#define SENTINEL_OK                         0x00000000u  /* Success            */
/* B+ Fatal */
#define SENTINEL_ERR_INVALID_MAGIC          0x0011A000u  /* Bad magic number   */
#define SENTINEL_ERR_UNSUPPORTED_VER        0x0011A001u  /* Unsupported format */
#define SENTINEL_ERR_HEADER_CRC             0x0011A002u  /* Header CRC corrupt */
#define SENTINEL_ERR_CHECKSUM_MISMATCH      0x0011A003u  /* State cksum fail   */
#define SENTINEL_ERR_DIRECTIVE_FAILED       0x0011A004u  /* Directive failed   */
#define SENTINEL_ERR_UNRECOVERABLE          0x0011A005u  /* Unrecoverable      */
#define SENTINEL_ERR_NO_RECOVERY_SCRIPT     0x0011A006u  /* No recovery script */
#define SENTINEL_ERR_BANCODE_OUT_RANGE      0x0011A007u  /* BANcode out of B+  */
#define SENTINEL_ERR_DEPENDENCY_MISSING     0x0011A008u  /* Missing dependency */
/* W+ Warning */
#define SENTINEL_ERR_TIMEOUT                0x0011A800u  /* Recovery timeout   */

/* ------------------------------------------------------------------ */
/*  State Checksum Entry (16 bytes each)                               */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t address;           /* Memory/sector address to checksum   */
    uint32_t size;              /* Size in bytes to cover              */
    uint16_t expected_value_lo; /* Expected checksum (low 16 bits)     */
    uint16_t expected_value_hi; /* Expected checksum (high 16 bits)    */
    uint8_t  checksum_type;     /* SENTINEL_CKSUM_* constant           */
    uint8_t  reserved[3];       /* Must be zero                        */
} sentinel_checksum_entry_t;

/* ------------------------------------------------------------------ */
/*  Fallback Trigger Entry (16 bytes each)                             */
/*  (Cross-ref: BANcode trap system)                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t bancode;           /* BANcode that triggers this fallback */
    uint32_t directive_index;   /* Index into directive array           */
    uint16_t max_retries;       /* Max retry attempts before abort     */
    uint16_t retry_count;       /* Current retry count (runtime)       */
    uint32_t flags;             /* Trigger-specific flags              */
} sentinel_trigger_entry_t;

#define SENTINEL_TRIGGER_FLAG_ABORT_ON_MAX  0x01u
#define SENTINEL_TRIGGER_FLAG_LOG_ONLY      0x02u
#define SENTINEL_TRIGGER_FLAG_REBOOT        0x04u

/* ------------------------------------------------------------------ */
/*  Directive Block Header (16 bytes, precedes each opcode payload)    */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t  opcode;            /* SENTINEL_OP_* constant              */
    uint8_t  operand_size;      /* Size of operand data following      */
    uint16_t condition_flags;   /* SENTINEL_COND_* bitfield            */
    uint32_t operand_offset;    /* Offset from header start to operand */
    uint32_t checksum_index;    /* Index to state checksum (or 0xFFFF) */
    uint32_t reserved;          /* Must be zero                        */
} sentinel_directive_header_t;

/* ------------------------------------------------------------------ */
/*  .sentinel Binary Image Header (128 bytes, at offset 0x0)          */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 0x00 */ uint32_t magic;                /* Must be SENTINEL_MAGIC     */
    /* 0x04 */ uint16_t format_version;       /* SENTINEL_FORMAT_VERSION    */
    /* 0x06 */ uint16_t header_size;          /* Must be SENTINEL_HEADER_SIZE */
    /* 0x08 */ uint32_t image_size;           /* Total file size in bytes   */
    /* 0x0C */ uint32_t header_checksum;      /* CRC32c of bytes 0x10..0x7F */
    /* 0x10 */ uint32_t trigger_bancode;      /* B+ code that fires this    */
    /* 0x14 */ uint32_t trigger_trap_index;   /* Trap slot for this script  */
    /* 0x18 */ uint32_t directive_count;      /* Number of directives       */
    /* 0x1C */ uint32_t checksum_count;       /* Number of state checksums  */
    /* 0x20 */ uint32_t trigger_count;        /* Number of fallback triggers*/
    /* 0x24 */ uint32_t directive_table_offset; /* Offset to directive table */
    /* 0x28 */ uint32_t checksum_table_offset;  /* Offset to checksum table */
    /* 0x2C */ uint32_t trigger_table_offset;   /* Offset to trigger table  */
    /* 0x30 */ uint32_t string_table_offset;    /* Offset to string table   */
    /* 0x34 */ uint32_t string_table_size;      /* Size of string table (B) */
    /* 0x38 */ uint64_t timestamp;             /* Build timestamp (UTC)     */
    /* 0x40 */ uint32_t target_volume_lba;     /* LBA of affected volume    */
    /* 0x44 */ uint32_t target_volume_size;    /* Sectors in volume         */
    /* 0x48 */ uint32_t associated_owc_offset; /* Offset to driver ref     */
    /* 0x4C */ uint32_t associated_owd_offset; /* Offset to library ref    */
    /* 0x50 */ uint32_t max_recovery_time_ms;  /* Max allowed recovery ms  */
    /* 0x54 */ uint32_t flags;                 /* Sentinel-wide flags       */
    /* 0x58 */ uint32_t image_checksum;        /* CRC32c of full file       */
    /* 0x5C */ uint32_t padding[8];            /* Pad to 128 bytes, zeros   */
} sentinel_header_t;

/* ------------------------------------------------------------------ */
/*  Sentinel Flags                                                    */
/* ------------------------------------------------------------------ */

#define SENTINEL_FLAG_NONE              0x00000000u
#define SENTINEL_FLAG_BOOT_ONLY         0x00000001u  /* Only valid     */
                                                     /* during boot    */
#define SENTINEL_FLAG_HOT_RECOVERY      0x00000002u  /* Can run while  */
                                                     /* system is live */
#define SENTINEL_FLAG_ABORT_ON_FAIL     0x00000004u  /* Halt on any    */
                                                     /* directive fail */
#define SENTINEL_FLAG_LOG_VERBOSE       0x00000008u  /* Verbose logging*/
#define SENTINEL_FLAG_REQUIRE_VIP       0x00000010u  /* Needs VIP      */
#define SENTINEL_FLAG_REQUIRE_HTLCALL   0x00000020u  /* Needs HTL DLL  */

/* ------------------------------------------------------------------ */
/*  Convenience: Validate a .sentinel header in place                 */
/* ------------------------------------------------------------------ */

static inline bool sentinel_header_valid(const sentinel_header_t *h)
{
    if (!h) return false;
    if (h->magic != SENTINEL_MAGIC) return false;
    if (h->header_size != SENTINEL_HEADER_SIZE) return false;
    if (h->directive_count > SENTINEL_MAX_DIRECTIVES) return false;
    if (h->checksum_count > SENTINEL_MAX_CHECKSUMS) return false;
    if (h->trigger_count > SENTINEL_MAX_TRIGGERS) return false;
    if (h->trigger_bancode < SENTINEL_BANCODE_RANGE_LO ||
        h->trigger_bancode > SENTINEL_BANCODE_RANGE_HI) return false;
    if (h->trigger_trap_index < (SENTINEL_TRAP_BASE >> 28) ||
        h->trigger_trap_index > (SENTINEL_TRAP_END >> 28)) return false;
    return true;
}

#endif /* SENTINEL_FORMAT_H */
