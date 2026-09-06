/*
 * damage_control.h - Damage Control Sentinel Engine
 *
 * Parses and executes .sentinel recovery directive files following a
 * system crash triggered by a BANcode panic. Evaluates corrupt volume
 * indexes via UniVIP/FVIP hooks, executes automated recovery passes,
 * and logs state to boot storage.
 *
 * C99 freestanding - caller-provided block memory, zero heap allocation.
 */

#ifndef DAMAGE_CONTROL_H
#define DAMAGE_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "../sentinel_format.h"

/* ------------------------------------------------------------------ */
/*  Forward declarations                                              */
/* ------------------------------------------------------------------ */

typedef struct dc_context       dc_context_t;
typedef struct dc_recovery_log  dc_recovery_log_t;

/* ------------------------------------------------------------------ */
/*  Recovery Engine Configuration                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t max_log_entries;       /* Max entries in recovery log      */
    uint32_t max_retry_per_trigger; /* Default max retries per trigger */
    uint32_t recovery_timeout_ms;   /* Global recovery timeout          */
    bool     abort_on_first_fail;   /* Halt on any directive failure    */
    bool     verbose_logging;       /* Enable verbose diagnostic output */
} dc_config_t;

/* ------------------------------------------------------------------ */
/*  Recovery Result — BANcode mapped                                   */
/*  B+ (0x0011A000-0x0011A77F): Fatal recovery failures               */
/*  W+ (0x0011A800-0x0011ABFF): Partial / non-fatal                   */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / informational                  */
/* ------------------------------------------------------------------ */

typedef uint32_t dc_recovery_result_t;

#define DC_RECOVERY_SUCCESS             0x00000000u  /* All directives passed */
/* W+ Warning */
#define DC_RECOVERY_PARTIAL             0x0011A800u  /* Some directives failed */
#define DC_RECOVERY_TIMEOUT             0x0011A801u  /* Recovery timeout      */
/* B+ Fatal */
#define DC_RECOVERY_FAILED              0x0011A000u  /* Recovery aborted      */
#define DC_RECOVERY_UNRECOVERABLE       0x0011A001u  /* System cannot continue */
#define DC_RECOVERY_NO_SCRIPT           0x0011A002u  /* No .sentinel file     */
#define DC_RECOVERY_INVALID_SCRIPT      0x0011A003u  /* .sentinel corrupted   */
#define DC_RECOVERY_CHECKSUM_FAIL       0x0011A004u  /* Checksum mismatch     */

/* ------------------------------------------------------------------ */
/*  Directive Execution Status — BANcode mapped                        */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal directive issues            */
/*  B+ (0x0011A000-0x0011A77F): Fatal directive failures              */
/* ------------------------------------------------------------------ */

typedef uint32_t dc_directive_status_t;

#define DC_DIRECTIVE_OK                 0x00000000u  /* Directive passed     */
/* W+ Warning */
#define DC_DIRECTIVE_SKIPPED            0x0011A800u  /* Condition not met    */
/* B+ Fatal */
#define DC_DIRECTIVE_FAILED             0x0011A000u  /* Execution failed     */
#define DC_DIRECTIVE_CRITICAL_FAIL      0x0011A001u  /* Critical fail        */

/* ------------------------------------------------------------------ */
/*  Log Entry Types                                                   */
/* ------------------------------------------------------------------ */

#define DC_LOG_TYPE_INFO        0x00u
#define DC_LOG_TYPE_WARN        0x01u
#define DC_LOG_TYPE_ERROR       0x02u
#define DC_LOG_TYPE_DIRECTIVE   0x03u
#define DC_LOG_TYPE_CHECKSUM    0x04u
#define DC_LOG_TYPE_BANCODE     0x05u
#define DC_LOG_TYPE_RECOVERY    0x06u

/* ------------------------------------------------------------------ */
/*  Log Entry                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    uint64_t timestamp;            /* Timestamp at time of log entry   */
    uint32_t bancode;              /* Associated BANcode (if any)      */
    uint8_t  type;                 /* DC_LOG_TYPE_* constant           */
    uint8_t  directive_index;      /* Directive index (if applicable)  */
    uint16_t message_offset;       /* Offset into string table         */
    uint32_t detail;               /* Additional numeric detail        */
} dc_log_entry_t;

/* ------------------------------------------------------------------ */
/*  Recovery Log                                                      */
/* ------------------------------------------------------------------ */

struct dc_recovery_log {
    dc_log_entry_t *entries;       /* Caller-provided buffer           */
    uint32_t capacity;             /* Max entries                      */
    uint32_t count;                /* Current entry count              */
    uint32_t string_table_offset;  /* Offset to string data in buffer  */
    uint32_t string_table_used;    /* Bytes used in string area        */
};

/* ------------------------------------------------------------------ */
/*  Context (opaque to callers after init)                             */
/* ------------------------------------------------------------------ */

struct dc_context {
    const sentinel_header_t       *header;        /* Parsed header    */
    const sentinel_directive_header_t *directives; /* Directive array */
    const sentinel_checksum_entry_t   *checksums;  /* Checksum array  */
    const sentinel_trigger_entry_t    *triggers;   /* Trigger array   */

    dc_config_t                    config;
    dc_recovery_log_t             *log;

    /* Runtime state */
    uint32_t                       current_directive;
    uint32_t                       total_directives_executed;
    uint32_t                       total_directives_failed;
    uint32_t                       total_checksums_validated;
    uint32_t                       total_checksums_failed;
    bool                           recovery_aborted;
    dc_recovery_result_t           final_result;

    /* Pointer to raw image bytes (for operand access) */
    const uint8_t                 *image_base;
    uint32_t                       image_size;
};

/* ------------------------------------------------------------------ */
/*  Initialization & Lifecycle                                        */
/* ------------------------------------------------------------------ */

/*
 * Initialize a recovery context from a .sentinel image in memory.
 * Validates the header and sets up internal pointers.
 * `image` must point to a complete .sentinel file in memory.
 * `image_size` is the total byte count.
 * `ctx` is caller-provided and zeroed by this function on success.
 * Returns SENTINEL_OK on success.
 */
sentinel_status_t dc_context_init(
    dc_context_t       *ctx,
    const uint8_t      *image,
    uint32_t            image_size,
    const dc_config_t  *config,
    dc_recovery_log_t  *log
);

/*
 * Release resources held by the context. Does not free `ctx` itself.
 */
void dc_context_release(dc_context_t *ctx);

/* ------------------------------------------------------------------ */
/*  Log Operations                                                    */
/* ------------------------------------------------------------------ */

/*
 * Initialize a recovery log. `buffer` must be at least `buffer_size`
 * bytes. The first `max_entries * sizeof(dc_log_entry_t)` bytes are
 * used for log entries; the remainder is the string table.
 */
void dc_log_init(
    dc_recovery_log_t  *log,
    dc_log_entry_t     *buffer,
    uint32_t            max_entries,
    uint32_t            buffer_size
);

/*
 * Append a formatted log entry. Returns true on success, false if
 * the log is full. `message` is a NUL-terminated SUTF-8 transport
 * string copied into the string table.
 */
bool dc_log_append(
    dc_recovery_log_t  *log,
    uint8_t             type,
    uint32_t            bancode,
    uint8_t             directive_index,
    const char         *message,
    uint32_t            detail
);

/* ------------------------------------------------------------------ */
/*  Checksum Validation                                               */
/* ------------------------------------------------------------------ */

/*
 * Validate a single state checksum entry. Reads `size` bytes from
 * `address`, computes the checksum, and compares against expected.
 * Returns true if the checksum matches.
 */
bool dc_validate_checksum(const sentinel_checksum_entry_t *entry);

/*
 * Validate all checksums defined in the .sentinel header.
 * Populates the log with results. Returns the number of failures.
 */
uint32_t dc_validate_all_checksums(dc_context_t *ctx);

/* ------------------------------------------------------------------ */
/*  Directive Execution                                               */
/* ------------------------------------------------------------------ */

/*
 * Execute a single directive block. Dispatches based on opcode.
 * Returns DC_DIRECTIVE_OK, DC_DIRECTIVE_SKIPPED, or DC_DIRECTIVE_FAILED.
 */
dc_directive_status_t dc_execute_directive(
    dc_context_t                       *ctx,
    const sentinel_directive_header_t  *dir,
    const uint8_t                      *operand_data
);

/*
 * Execute all directives in sequence, honoring conditions and retries.
 * Returns the overall recovery result.
 */
dc_recovery_result_t dc_execute_all(dc_context_t *ctx);

/* ------------------------------------------------------------------ */
/*  Trigger Evaluation                                                */
/* ------------------------------------------------------------------ */

/*
 * Check if a given BANcode matches any trigger in the .sentinel file.
 * If matched, returns the trigger entry and sets `directive_index_out`
 * to the starting directive index. Returns true if a trigger matched.
 */
bool dc_find_trigger(
    const dc_context_t         *ctx,
    uint32_t                    bancode,
    const sentinel_trigger_entry_t **trigger_out,
    uint32_t                   *directive_index_out
);

/* ------------------------------------------------------------------ */
/*  Recovery Pass Orchestration                                        */
/* ------------------------------------------------------------------ */

/*
 * Top-level recovery entry point. Call this after a BANcode panic
 * with the .sentinel image loaded. Orchestrates:
 *   1. Header validation
 *   2. State checksum verification
 *   3. Trigger matching
 *   4. Directive execution with retries
 *   5. Logging all results
 * Returns the final recovery outcome.
 */
dc_recovery_result_t dc_run_recovery(
    const uint8_t      *sentinel_image,
    uint32_t            image_size,
    const dc_config_t  *config,
    dc_recovery_log_t  *log
);

#endif /* DAMAGE_CONTROL_H */
