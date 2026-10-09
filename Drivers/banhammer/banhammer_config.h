/*
 * banhammer_config.h - Banhammer Configuration Block Format
 *
 * Defines the BHCF (Banhammer Configuration) binary format embedded
 * within .owc driver images or loaded from boot storage. Controls
 * post-panic behavior: auto-reboot, memory dump, severity thresholds,
 * and telemetry routing.
 *
 * C99 freestanding - no dynamic allocation.
 */

#ifndef BANHAMMER_CONFIG_H
#define BANHAMMER_CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ------------------------------------------------------------------ */
/*  BHCF Magic & Layout                                                */
/* ------------------------------------------------------------------ */

#define BHCF_MAGIC              0x42484346u  /* "BHCF" little-endian    */
#define BHCF_HEADER_SIZE        64u          /* Fixed header size       */
#define BHCF_FORMAT_VERSION     0x0001u      /* Major.Minor = 1.0       */
#define BHCF_MAX_REGIONS        8u           /* Max dump regions        */
#define BHCF_CHECKSUM_SEED      0xDEADBEEFu /* CRC32 initial seed      */

/* ------------------------------------------------------------------ */
/*  Atomic Behavior Flags                                              */
/*  Stored as a bitmask in flags field. The semantic is:               */
/*    0x00 = Neither (halt only)                                       */
/*    0x01 = Auto-Reboot after telemetry write                         */
/*    0x02 = Memory Dump to storage before halt                        */
/*    0x03 = Both (dump then reboot)                                   */
/* ------------------------------------------------------------------ */

#define BHCF_FLAG_NONE          0x00u
#define BHCF_FLAG_AUTO_REBOOT   0x01u
#define BHCF_FLAG_MEMORY_DUMP   0x02u
#define BHCF_FLAG_BOTH          (BHCF_FLAG_AUTO_REBOOT | BHCF_FLAG_MEMORY_DUMP)

/* Extended flags (bit positions 2-7) */
#define BHCF_FLAG_TELEMETRY_ON  0x04u  /* Write telemetry ring to storage */
#define BHCF_FLAG_SERIAL_LOG    0x08u  /* Echo panic to serial port       */
#define BHCF_FLAG_SENTINEL_HOOK 0x10u  /* Invoke .sentinel recovery after */
#define BHCF_FLAG_SCREEN_PANIC  0x20u  /* Draw panic screen via bootvid   */

/* ------------------------------------------------------------------ */
/*  Panic Severity Levels                                              */
/* ------------------------------------------------------------------ */

#define BHCF_SEVERITY_INFO          0x00u  /* Informational, no halt     */
#define BHCF_SEVERITY_WARNING       0x01u  /* Non-fatal, telemetry only  */
#define BHCF_SEVERITY_RECOVERABLE   0x02u  /* Recoverable fault          */
#define BHCF_SEVERITY_CRITICAL      0x03u  /* System halt required       */
#define BHCF_SEVERITY_CATASTROPHIC  0x04u  /* Immediate emergency exec   */

/* ------------------------------------------------------------------ */
/*  Dump Region Descriptor (16 bytes each)                             */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t base_address;      /* Physical/virtual address to dump     */
    uint32_t size_bytes;        /* Size of region in bytes              */
    uint8_t  region_type;       /* 0=general, 1=register, 2=MMIO       */
    uint8_t  reserved[3];       /* Must be zero                         */
} bhcf_dump_region_t;

/* ------------------------------------------------------------------ */
/*  BHCF Configuration Block (64 bytes, at offset 0x0)                */
/* ------------------------------------------------------------------ */

typedef struct {
    /* 0x00 */ uint32_t magic;              /* Must be BHCF_MAGIC          */
    /* 0x04 */ uint16_t format_version;     /* BHCF_FORMAT_VERSION         */
    /* 0x08 */ uint16_t header_size;        /* Must be BHCF_HEADER_SIZE    */
    /* 0x08 */ uint8_t  behavior_flags;     /* BHCF_FLAG_* bitmask         */
    /* 0x09 */ uint8_t  severity_threshold; /* BHCF_SEVERITY_* : halt at   */
                                           /* this level or above          */
    /* 0x0A */ uint16_t reboot_delay_ms;    /* Delay before reboot (0=instant) */
    /* 0x0C */ uint32_t max_dump_size;      /* Max bytes for memory dump   */
    /* 0x10 */ uint32_t telemetry_lba;      /* LBA on boot vol for telemetry */
    /* 0x14 */ uint32_t telemetry_sectors;  /* Sector count reserved       */
    /* 0x18 */ uint8_t  dump_region_count;  /* Number of dump regions      */
    /* 0x19 */ uint8_t  reserved_0;         /* Must be zero                */
    /* 0x1A */ uint16_t reserved_1;         /* Must be zero                */
    /* 0x1C */ uint32_t config_checksum;    /* CRC32c of fields 0x08-0x3F  */
    /* 0x20 */ uint32_t image_checksum;     /* CRC32c of full BHCF block   */
    /* 0x24 */ uint32_t associated_bancode; /* BANcode this config governs  */
    /* 0x28 */ uint32_t reserved_2[6];      /* Zero-padded to 64 bytes     */
} bhcf_config_t;

/* ------------------------------------------------------------------ */
/*  Configuration Status — BANcode mapped                              */
/*  B+ (0x0011A000-0x0011A77F): Fatal config faults                   */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
/* ------------------------------------------------------------------ */

typedef uint32_t bhcf_status_t;

#define BHCF_OK                           0x00000000u  /* Success              */
/* B+ Fatal */
#define BHCF_ERR_INVALID_MAGIC            0x0011A000u  /* Bad magic number     */
#define BHCF_ERR_UNSUPPORTED_VER          0x0011A001u  /* Unsupported version  */
#define BHCF_ERR_HEADER_CRC               0x0011A002u  /* Header CRC corrupt   */
#define BHCF_ERR_CONFIG_CRC               0x0011A003u  /* Config CRC corrupt   */
/* S+ Soft */
#define BHCF_ERR_NULL_POINTER             0x0011AE00u  /* Null pointer         */
#define BHCF_ERR_INVALID_FLAGS            0x0011AE01u  /* Invalid flags        */
#define BHCF_ERR_INVALID_SEVERITY         0x0011AE02u  /* Invalid severity     */
#define BHCF_ERR_INVALID_REGIONS          0x0011AE03u  /* Invalid dump regions */

/* ------------------------------------------------------------------ */
/*  Configuration API                                                  */
/* ------------------------------------------------------------------ */

/*
 * Validate a BHCF block in memory. Checks magic, version, header size,
 * config CRC, and image CRC. Returns BHCF_OK on success.
 */
bhcf_status_t bhcf_validate(const bhcf_config_t *cfg);

/*
 * Initialize a BHCF block with safe defaults:
 *   behavior_flags = BHCF_FLAG_BOTH (dump + reboot)
 *   severity_threshold = BHCF_SEVERITY_CRITICAL
 *   reboot_delay_ms = 1000
 *   All other fields zeroed.
 * Computes checksums. Returns BHCF_OK.
 */
bhcf_status_t bhcf_init_defaults(bhcf_config_t *cfg);

/*
 * Check if a specific behavior flag is set.
 */
bool bhcf_flag_set(const bhcf_config_t *cfg, uint8_t flag);

/*
 * Set or clear a behavior flag. Recomputes config checksum.
 */
bhcf_status_t bhcf_set_flag(bhcf_config_t *cfg, uint8_t flag, bool enable);

/*
 * Check if the given severity level triggers a halt based on config.
 */
bool bhcf_severity_triggers_halt(const bhcf_config_t *cfg, uint8_t severity);

/*
 * Recompute the config checksum (fields 0x08-0x3F) and image checksum
 * (full block). Must be called after any modification.
 */
void bhcf_recompute_checksums(bhcf_config_t *cfg);

/*
 * Get a human-readable name for a behavior flag.
 */
const char *bhcf_flag_name(uint8_t flag);

/*
 * Get a human-readable name for a severity level.
 */
const char *bhcf_severity_name(uint8_t severity);

#endif /* BANHAMMER_CONFIG_H */
