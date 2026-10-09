#ifndef SUCS_PLUGIN_H
#define SUCS_PLUGIN_H

/**
 * SuperUnicode Plugin Subsystem (ExtSUCS Plugin Component)
 *
 * ExtSUCS defines an unbounded (0 -> infinity) character encoding address
 * space. In website / default runtime configurations the practical ceiling
 * is the Base SUCS 31-bit limit (0x7FFFFFFF). SuperUnicode Plugins extend
 * the ACTIVE codepoint namespace BEYOND that base limit.
 *
 * Plugin lifecycle (mirrors the Base <-> Extended mode-switch subsystem):
 *
 *   1. STAGE (runtime):  sucs_plugin_stage_install() validates the plugin
 *      blob structure, stages it, and returns SUCS_PLUGIN_REBOOT_REQUIRED.
 *      Plugin installation ALWAYS requires a mandatory system restart.
 *
 *   2. BOOT  (early kernel boot): sucs_plugin_commit_on_boot() re-checks
 *      every staged plugin against its stored checksum (CRC32c +
 *      Fletcher-64). ONLY valid plugins are mounted and their codepoint
 *      ranges registered into the active ExtSUCS namespace. Invalid
 *      plugins are quarantined and NEVER mounted.
 *
 *   3. MOUNT (after the checksum pass): each valid plugin is mounted as a
 *      SuperUnicode PLUGIN Partition. Plugin partitions MUST be formatted
 *      exclusively as OWFS (OpenWindows File System) and are read-only.
 *
 * SuperUnicode Partition policy:
 *   - SuperUnicode Partitions (base): OWFS or USFS. Used by Base SUCS
 *     ONLY for bugfix + rescue payloads.
 *   - SuperUnicode Plugin Partitions (Extended-only): OWFS exclusively.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "extsucs_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define SUCS_PLUGIN_PACKED __attribute__((packed))
#else
#define SUCS_PLUGIN_PACKED
#endif

/* ============================================================================
 * Plugin Blob Format Constants
 * ============================================================================ */
#define SUCS_PLUGIN_BLOB_MAGIC     0x53435343UL    /* "SUCS" */
#define SUCS_PLUGIN_BLOB_VERSION   1u              /* Current blob format version */
#define SUCS_PLUGIN_ID_MAX         64u             /* Plugin identifier max length */
#define SUCS_PLUGIN_MAX_RANGES     64u             /* Max codepoint ranges per plugin */
#define SUCS_PLUGIN_MAX_PENDING    4u              /* Max staged plugins awaiting reboot */
#define SUCS_PLUGIN_MAX_MOUNTED    16u             /* Max mounted plugin partitions */
#define SUCS_PLUGIN_MAX_REGISTERED_RANGES 256u     /* Max registered ranges across plugins */
#define SUCS_PLUGIN_STAGE_BUF_SIZE (16u * 1024u)   /* Staging buffer size per plugin */

/* Default base limit: the practical runtime ceiling of ExtSUCS on websites /
 * default configurations. Plugins add codepoints ABOVE this limit. */
#define SUCS_PLUGIN_BASE_LIMIT  ((sucs_ex_char_t)0x7FFFFFFFULL)

/* ============================================================================
 * Codepoint Range (one plugin-owned block of the ExtSUCS namespace)
 * ============================================================================ */
typedef struct {
    sucs_ex_char_t start;   /* Inclusive range start (> SUCS_PLUGIN_BASE_LIMIT) */
    sucs_ex_char_t end;     /* Inclusive range end */
} sucs_plugin_range_t;

/* ============================================================================
 * Codepoint Data Entry (machine instructions carried by a plugin)
 * ============================================================================ */
typedef struct {
    sucs_ex_char_t cp;        /* Plugin-owned codepoint */
    const char*    name;      /* Human / machine-readable name */
    uint32_t       type;      /* 0 = printable native allocation, 1 = control/instruction */
} sucs_plugin_data_entry_t;

/* ============================================================================
 * Plugin ABI (what a plugin author must provide)
 *
 * The ONE hard requirement for a plugin is sucs_plugin_entry(), returning a
 * pointer to a statically-initialized sucs_plugin_t. See the SDK template.
 * ============================================================================ */
typedef struct {
    char                    id[SUCS_PLUGIN_ID_MAX]; /* Unique plugin identifier */
    uint8_t                 ver_major;              /* Semantic version major */
    uint8_t                 ver_minor;              /* Semantic version minor */
    uint8_t                 ver_patch;              /* Semantic version patch */
    sucs_ex_char_t          base_limit;             /* Runtime ceiling (default 0x7FFFFFFF) */
    uint32_t                range_count;            /* Number of codepoint ranges */
    const sucs_plugin_range_t* ranges;              /* Codepoint range table (> base_limit) */
    const sucs_plugin_data_entry_t* data;           /* Optional codepoint data table */
    uint32_t                data_count;             /* Optional data table length */
} sucs_plugin_t;

/* ============================================================================
 * On-Disk / On-Slot Plugin Blob Header (packed wire format)
 *
 * Layout: header (this struct) | range_count * 16-byte ranges | payload bytes.
 * crc32c and fletcher64 are computed over the ENTIRE blob with the two
 * checksum fields zeroed.
 * ============================================================================ */
typedef struct SUCS_PLUGIN_PACKED {
    uint32_t    magic;          /* SUCS_PLUGIN_BLOB_MAGIC */
    uint16_t    blob_version;   /* SUCS_PLUGIN_BLOB_VERSION */
    uint8_t     ver_major;      /* Plugin semantic version major */
    uint8_t     ver_minor;      /* Plugin semantic version minor */
    uint8_t     ver_patch;      /* Plugin semantic version patch */
    uint8_t     reserved;       /* Reserved (zero) */
    char        id[SUCS_PLUGIN_ID_MAX];  /* Plugin identifier (nul-terminated) */
    uint32_t    range_count;    /* Number of 16-byte range records following */
    uint32_t    blob_size;      /* Total blob size in bytes */
    uint32_t    crc32c;         /* CRC32c (Castagnoli) — zeroed during computation */
    uint64_t    fletcher64;     /* Fletcher-64 — zeroed during computation */
} sucs_plugin_blob_header_t;

/* ============================================================================
 * Plugin Status & Return Codes
 * ============================================================================ */
typedef enum {
    SUCS_PLUGIN_OK                    = 0,   /* Operation succeeded */
    SUCS_PLUGIN_ERR_INVALID_BLOB      = 1,   /* Malformed plugin blob */
    SUCS_PLUGIN_ERR_INVALID_ID        = 2,   /* Empty / malformed plugin id */
    SUCS_PLUGIN_ERR_INVALID_RANGE     = 3,   /* Malformed codepoint range */
    SUCS_PLUGIN_ERR_RANGE_BELOW_BASE  = 4,   /* Range does not extend past base limit */
    SUCS_PLUGIN_ERR_RANGE_COLLISION   = 5,   /* Range overlaps an active plugin */
    SUCS_PLUGIN_ERR_CHECKSUM_MISMATCH = 6,   /* Stored checksum does not match blob */
    SUCS_PLUGIN_ERR_NOT_OWFS          = 7,   /* Plugin partition must be OWFS */
    SUCS_PLUGIN_ERR_MOUNT_FAILED      = 8,   /* Partition mount failed */
    SUCS_PLUGIN_ERR_BUFFER_TOO_SMALL  = 9,   /* Blob exceeds staging capacity */
    SUCS_PLUGIN_ERR_DUPLICATE_ID      = 10,  /* Plugin id already staged / active */
    SUCS_PLUGIN_ERR_STAGING_FULL      = 11,  /* Staging table is full */
    SUCS_PLUGIN_ERR_UNSUPPORTED_VERSION = 12, /* Blob format version unsupported */
    SUCS_PLUGIN_REBOOT_REQUIRED       = 13   /* Plugin staged; system restart required */
} sucs_plugin_status_t;

#ifdef __cplusplus
}
#endif

#endif /* SUCS_PLUGIN_H */
