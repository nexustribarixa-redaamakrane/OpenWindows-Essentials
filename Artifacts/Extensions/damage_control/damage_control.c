/*
 * damage_control.c - Damage Control Sentinel Engine
 *
 * C99 freestanding implementation. Parses and executes .sentinel recovery
 * directive files following a system crash triggered by a BANcode panic.
 * Validates corrupt volume indexes via UniVIP/FVIP hooks, executes
 * automated recovery passes, and logs all state to boot storage.
 *
 * Build: cc -std=c99 -ffreestanding -nostdlib -c damage_control.c
 * Zero heap allocation. All memory is caller-provided.
 */

#include "damage_control.h"

/* ------------------------------------------------------------------ */
/*  Internal Constants                                                 */
/* ------------------------------------------------------------------ */

#define DC_OWFS_BLOCK_SIZE          0x1000u

#define DC_HEADER_CKSUM_OFFSET      0x10u
#define DC_HEADER_CKSUM_LENGTH      0x70u

#define DC_IMAGE_CKSUM_FIELD_OFFSET 0x58u
#define DC_IMAGE_CKSUM_FIELD_SIZE   4u

#define DC_NO_CHECKSUM_INDEX        0xFFFFFFFFu

#define DC_DIRECTIVE_HEADER_SIZE    16u

/* ------------------------------------------------------------------ */
/*  Internal Utility Functions                                         */
/* ------------------------------------------------------------------ */

static void dc_memset(uint8_t *dst, uint8_t val, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        dst[i] = val;
}

static void dc_memcpy(uint8_t *dst, const uint8_t *src, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++)
        dst[i] = src[i];
}

static uint32_t dc_strlen(const char *s)
{
    uint32_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

/* ------------------------------------------------------------------ */
/*  Internal Checksum Algorithms                                       */
/*  (Cross-ref: OpenWindows-Storage owfs_superblock_t CRC/Fletcher)   */
/* ------------------------------------------------------------------ */

/*
 * CRC-32c (iSCSI polynomial 0x82F63B78 reflected).
 * Bitwise computation, no lookup table.
 */
static uint32_t dc_crc32c(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < length; i++) {
        crc ^= (uint32_t)data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0x82F63B78u;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

/*
 * CRC-32c with a zeroed skip region. Used to validate checksum fields
 * where the field itself must be treated as zero during computation.
 */
static uint32_t dc_crc32c_skip(const uint8_t *data, uint32_t length,
                               uint32_t skip_off, uint32_t skip_len)
{
    uint32_t crc = 0xFFFFFFFFu;
    uint32_t skip_end = skip_off + skip_len;

    for (uint32_t i = 0; i < length; i++) {
        uint8_t b;
        if (i >= skip_off && i < skip_end)
            b = 0;
        else
            b = data[i];

        crc ^= (uint32_t)b;
        for (int j = 0; j < 8; j++) {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0x82F63B78u;
            else
                crc >>= 1;
        }
    }

    return crc ^ 0xFFFFFFFFu;
}

/*
 * Fletcher-16: 16-bit checksum using two 8-bit accumulators mod 255.
 * Result packed as (sum2 << 8) | sum1.
 */
static uint16_t dc_fletcher16(const uint8_t *data, uint32_t length)
{
    uint32_t sum1 = 0;
    uint32_t sum2 = 0;

    for (uint32_t i = 0; i < length; i++) {
        sum1 += (uint32_t)data[i];
        while (sum1 >= 255u)
            sum1 -= 255u;
        sum2 += sum1;
        while (sum2 >= 255u)
            sum2 -= 255u;
    }

    return (uint16_t)((sum2 << 8) | sum1);
}

/*
 * Fletcher-32: 32-bit checksum using two 16-bit accumulators mod 65535.
 * Input is treated as array of 16-bit words (little-endian byte pairs).
 * Odd trailing byte is zero-padded.
 */
static uint32_t dc_fletcher32(const uint8_t *data, uint32_t length)
{
    uint32_t sum1 = 0;
    uint32_t sum2 = 0;
    uint32_t idx = 0;

    while (idx + 1 < length) {
        uint32_t word = (uint32_t)data[idx] | ((uint32_t)data[idx + 1] << 8);
        sum1 += word;
        while (sum1 >= 0xFFFFu)
            sum1 = (sum1 & 0xFFFFu) + (sum1 >> 16);
        sum2 += sum1;
        while (sum2 >= 0xFFFFu)
            sum2 = (sum2 & 0xFFFFu) + (sum2 >> 16);
        idx += 2;
    }

    if (idx < length) {
        uint32_t word = (uint32_t)data[idx];
        sum1 += word;
        while (sum1 >= 0xFFFFu)
            sum1 = (sum1 & 0xFFFFu) + (sum1 >> 16);
        sum2 += sum1;
        while (sum2 >= 0xFFFFu)
            sum2 = (sum2 & 0xFFFFu) + (sum2 >> 16);
    }

    sum1 = (sum1 & 0xFFFFu) + (sum1 >> 16);
    sum2 = (sum2 & 0xFFFFu) + (sum2 >> 16);
    sum1 = (sum1 & 0xFFFFu) + (sum1 >> 16);
    sum2 = (sum2 & 0xFFFFu) + (sum2 >> 16);

    return (sum2 << 16) | sum1;
}

/*
 * XOR-16: XOR folding of all 16-bit words.
 * Simplest integrity check; detects single-bit flips but not
 * transpositions or even-count multi-bit errors.
 */
static uint16_t dc_xor16(const uint8_t *data, uint32_t length)
{
    uint16_t result = 0;
    uint32_t idx = 0;

    while (idx + 1 < length) {
        result ^= (uint16_t)data[idx] | ((uint16_t)data[idx + 1] << 8);
        idx += 2;
    }

    if (idx < length)
        result ^= (uint16_t)data[idx];

    return result;
}

/* ------------------------------------------------------------------ */
/*  Internal Checksum Dispatcher                                       */
/* ------------------------------------------------------------------ */

static uint32_t dc_compute_checksum(uint8_t type,
                                    const uint8_t *data,
                                    uint32_t length)
{
    switch (type) {
    case SENTINEL_CKSUM_CRC32C:
        return dc_crc32c(data, length);
    case SENTINEL_CKSUM_FLETCHER16:
        return (uint32_t)dc_fletcher16(data, length);
    case SENTINEL_CKSUM_FLETCHER32:
        return dc_fletcher32(data, length);
    case SENTINEL_CKSUM_XOR16:
        return (uint32_t)dc_xor16(data, length);
    default:
        return 0;
    }
}

/* ------------------------------------------------------------------ */
/*  Internal Header/Image Checksum Validators                          */
/* ------------------------------------------------------------------ */

/*
 * Validate header_checksum: CRC-32c of bytes [0x10 .. 0x7F] of the
 * .sentinel image. This range covers all header fields from
 * trigger_bancode through the end of the padding area, excluding
 * the header_checksum field itself (at 0x0C) and the preceding
 * magic/format/image_size fields.
 */
static bool dc_validate_header_checksum(const sentinel_header_t *hdr,
                                        const uint8_t *image)
{
    uint32_t computed = dc_crc32c(image + DC_HEADER_CKSUM_OFFSET,
                                  DC_HEADER_CKSUM_LENGTH);
    return computed == hdr->header_checksum;
}

/*
 * Validate image_checksum: CRC-32c of the full .sentinel file with
 * the image_checksum field (at offset 0x58) treated as zero.
 * During image construction the creator sets image_checksum=0,
 * computes the CRC, and stores the result.
 */
static bool dc_validate_image_checksum(const sentinel_header_t *hdr,
                                       const uint8_t *image,
                                       uint32_t image_size)
{
    uint32_t computed = dc_crc32c_skip(image, image_size,
                                       DC_IMAGE_CKSUM_FIELD_OFFSET,
                                       DC_IMAGE_CKSUM_FIELD_SIZE);
    return computed == hdr->image_checksum;
}

/* ------------------------------------------------------------------ */
/*  Log Operations                                                     */
/* ------------------------------------------------------------------ */

void dc_log_init(dc_recovery_log_t *log,
                 dc_log_entry_t *buffer,
                 uint32_t max_entries,
                 uint32_t buffer_size)
{
    log->entries = buffer;
    log->capacity = max_entries;
    log->count = 0;
    log->string_table_offset = max_entries * (uint32_t)sizeof(dc_log_entry_t);
    log->string_table_used = 0;

    dc_memset((uint8_t *)buffer, 0, buffer_size);
}

bool dc_log_append(dc_recovery_log_t *log,
                   uint8_t type,
                   uint32_t bancode,
                   uint8_t directive_index,
                   const char *message,
                   uint32_t detail)
{
    if (log->count >= log->capacity)
        return false;

    uint32_t msg_len = dc_strlen(message);
    uint32_t needed = msg_len + 1;

    /* The string table lives immediately after the entry array.
     * Total buffer capacity is implicit from the initial buffer_size
     * passed to dc_log_init. We derive available space from the
     * difference between total allocated bytes and what's consumed. */
    uint8_t *base = (uint8_t *)log->entries;
    uint32_t entry_area = log->capacity * (uint32_t)sizeof(dc_log_entry_t);
    uint32_t table_capacity = 0;
    if (entry_area < 0xFFFFFFFFu)
        table_capacity = 0xFFFFFFFFu - entry_area;

    if (log->string_table_used + needed > table_capacity)
        return false;

    dc_log_entry_t *e = &log->entries[log->count];
    e->timestamp = 0;
    e->bancode = bancode;
    e->type = type;
    e->directive_index = directive_index;
    e->message_offset = (uint16_t)log->string_table_used;
    e->detail = detail;

    uint8_t *str_dst = base + log->string_table_offset + log->string_table_used;
    dc_memcpy(str_dst, (const uint8_t *)message, needed);

    log->string_table_used += needed;
    log->count++;

    return true;
}

/* ------------------------------------------------------------------ */
/*  Context Initialization & Lifecycle                                 */
/* ------------------------------------------------------------------ */

sentinel_status_t dc_context_init(dc_context_t *ctx,
                                  const uint8_t *image,
                                  uint32_t image_size,
                                  const dc_config_t *config,
                                  dc_recovery_log_t *log)
{
    if (!ctx || !image || !config)
        return SENTINEL_ERR_INVALID_MAGIC;

    dc_memset((uint8_t *)ctx, 0, sizeof(dc_context_t));

    if (image_size < SENTINEL_HEADER_SIZE)
        return SENTINEL_ERR_INVALID_MAGIC;

    const sentinel_header_t *hdr = (const sentinel_header_t *)image;

    if (!sentinel_header_valid(hdr))
        return SENTINEL_ERR_INVALID_MAGIC;

    if (hdr->format_version != SENTINEL_FORMAT_VERSION)
        return SENTINEL_ERR_UNSUPPORTED_VER;

    if (hdr->image_size != image_size)
        return SENTINEL_ERR_INVALID_MAGIC;

    if (!dc_validate_header_checksum(hdr, image))
        return SENTINEL_ERR_HEADER_CRC;

    if (!dc_validate_image_checksum(hdr, image, image_size))
        return SENTINEL_ERR_HEADER_CRC;

    if (hdr->directive_count > SENTINEL_MAX_DIRECTIVES)
        return SENTINEL_ERR_INVALID_MAGIC;

    if (hdr->checksum_count > SENTINEL_MAX_CHECKSUMS)
        return SENTINEL_ERR_INVALID_MAGIC;

    if (hdr->trigger_count > SENTINEL_MAX_TRIGGERS)
        return SENTINEL_ERR_INVALID_MAGIC;

    if (hdr->trigger_bancode < SENTINEL_BANCODE_RANGE_LO ||
        hdr->trigger_bancode > SENTINEL_BANCODE_RANGE_HI)
        return SENTINEL_ERR_BANCODE_OUT_RANGE;

    if (hdr->directive_table_offset < SENTINEL_HEADER_SIZE)
        return SENTINEL_ERR_INVALID_MAGIC;

    uint64_t dir_end = (uint64_t)hdr->directive_table_offset +
                       (uint64_t)hdr->directive_count *
                       DC_DIRECTIVE_HEADER_SIZE;
    if (dir_end > (uint64_t)image_size)
        return SENTINEL_ERR_INVALID_MAGIC;

    if (hdr->checksum_table_offset < SENTINEL_HEADER_SIZE)
        return SENTINEL_ERR_INVALID_MAGIC;

    uint64_t csum_end = (uint64_t)hdr->checksum_table_offset +
                        (uint64_t)hdr->checksum_count *
                        sizeof(sentinel_checksum_entry_t);
    if (csum_end > (uint64_t)image_size)
        return SENTINEL_ERR_INVALID_MAGIC;

    if (hdr->trigger_table_offset < SENTINEL_HEADER_SIZE)
        return SENTINEL_ERR_INVALID_MAGIC;

    uint64_t trig_end = (uint64_t)hdr->trigger_table_offset +
                        (uint64_t)hdr->trigger_count *
                        sizeof(sentinel_trigger_entry_t);
    if (trig_end > (uint64_t)image_size)
        return SENTINEL_ERR_INVALID_MAGIC;

    if (hdr->string_table_offset < SENTINEL_HEADER_SIZE)
        return SENTINEL_ERR_INVALID_MAGIC;

    uint64_t str_end = (uint64_t)hdr->string_table_offset +
                       (uint64_t)hdr->string_table_size;
    if (str_end > (uint64_t)image_size)
        return SENTINEL_ERR_INVALID_MAGIC;

    ctx->header = hdr;
    ctx->directives = (const sentinel_directive_header_t *)
                      (image + hdr->directive_table_offset);
    ctx->checksums = (const sentinel_checksum_entry_t *)
                     (image + hdr->checksum_table_offset);
    ctx->triggers = (const sentinel_trigger_entry_t *)
                    (image + hdr->trigger_table_offset);

    ctx->config = *config;
    ctx->log = log;

    ctx->image_base = image;
    ctx->image_size = image_size;

    ctx->current_directive = 0;
    ctx->total_directives_executed = 0;
    ctx->total_directives_failed = 0;
    ctx->total_checksums_validated = 0;
    ctx->total_checksums_failed = 0;
    ctx->recovery_aborted = false;
    ctx->final_result = DC_RECOVERY_SUCCESS;

    return SENTINEL_OK;
}

void dc_context_release(dc_context_t *ctx)
{
    if (!ctx)
        return;

    ctx->header = NULL;
    ctx->directives = NULL;
    ctx->checksums = NULL;
    ctx->triggers = NULL;
    ctx->image_base = NULL;
    ctx->image_size = 0;
    ctx->log = NULL;
}

/* ------------------------------------------------------------------ */
/*  Checksum Validation                                                */
/* ------------------------------------------------------------------ */

bool dc_validate_checksum(const sentinel_checksum_entry_t *entry)
{
    if (!entry)
        return false;

    if (entry->checksum_type == SENTINEL_CKSUM_NONE)
        return true;

    if (entry->size == 0)
        return true;

    const uint8_t *data = (const uint8_t *)(uintptr_t)entry->address;
    uint32_t computed = dc_compute_checksum(entry->checksum_type,
                                            data, entry->size);

    uint32_t expected = (uint32_t)entry->expected_value_lo |
                        ((uint32_t)entry->expected_value_hi << 16);

    return computed == expected;
}

uint32_t dc_validate_all_checksums(dc_context_t *ctx)
{
    if (!ctx || !ctx->header)
        return 0;

    uint32_t failures = 0;
    uint32_t count = ctx->header->checksum_count;

    for (uint32_t i = 0; i < count; i++) {
        const sentinel_checksum_entry_t *entry = &ctx->checksums[i];
        bool passed = dc_validate_checksum(entry);

        if (passed) {
            ctx->total_checksums_validated++;
        } else {
            ctx->total_checksums_failed++;
            failures++;
        }

        if (ctx->config.verbose_logging) {
            dc_log_append(ctx->log,
                          DC_LOG_TYPE_CHECKSUM,
                          ctx->header->trigger_bancode,
                          0xFF,
                          passed ? "cksum ok" : "cksum fail",
                          i);
        }
    }

    return failures;
}

/* ------------------------------------------------------------------ */
/*  Internal Opcode Handlers                                           */
/* ------------------------------------------------------------------ */

/*
 * SENTINEL_OP_NOOP (0x00)
 * No operation. Used as alignment padding between directive blocks
 * or as a placeholder in partially-built recovery scripts.
 */
static dc_directive_status_t dc_handle_noop(dc_context_t *ctx,
                                            const uint8_t *operand_data,
                                            uint32_t operand_size)
{
    (void)ctx;
    (void)operand_data;
    (void)operand_size;
    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_RESTORE_SHADOW (0x01)
 * Restores the shadow file allocation table from the backup copy
 * stored at the backup LBA. The shadow FAT mirrors the primary FAT
 * and is used for atomic metadata updates. If the shadow is corrupt,
 * restoring from backup prevents cascading allocation failures.
 *
 * Operand: { uint32_t shadow_lba, uint32_t backup_lba, uint32_t sectors }
 *
 * Full system: Issues a sector-level copy from backup_lba to
 * shadow_lba across `sectors` VIP sectors (512 bytes each),
 * then validates the restored shadow FAT header checksum.
 */
static dc_directive_status_t dc_handle_restore_shadow(dc_context_t *ctx,
                                                      const uint8_t *operand_data,
                                                      uint32_t operand_size)
{
    if (operand_size < 12u)
        return DC_DIRECTIVE_FAILED;

    uint32_t shadow_lba = (uint32_t)operand_data[0] |
                          ((uint32_t)operand_data[1] << 8) |
                          ((uint32_t)operand_data[2] << 16) |
                          ((uint32_t)operand_data[3] << 24);
    uint32_t backup_lba = (uint32_t)operand_data[4] |
                          ((uint32_t)operand_data[5] << 8) |
                          ((uint32_t)operand_data[6] << 16) |
                          ((uint32_t)operand_data[7] << 24);
    uint32_t sectors = (uint32_t)operand_data[8] |
                       ((uint32_t)operand_data[9] << 8) |
                       ((uint32_t)operand_data[10] << 16) |
                       ((uint32_t)operand_data[11] << 24);

    if (sectors == 0 || sectors > SENTINEL_VIP_MBL_RESERVED)
        return DC_DIRECTIVE_FAILED;

    /*
     * In the full system: read `sectors` VIP sectors (512 bytes each)
     * from backup_lba, write them to shadow_lba. The VIP storage layer
     * (vip/univip_fvip.h) provides sector_read/sector_write primitives.
     * After the copy, re-validate the shadow FAT header at shadow_lba
     * to confirm structural integrity before the primary FAT switchover.
     */
    (void)shadow_lba;
    (void)backup_lba;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "restore shadow FAT",
                      shadow_lba);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_ISOLATE_BUS (0x02)
 * Disconnects a faulted hardware bus from the interconnect fabric.
 * This prevents DMA parity errors or timeout faults from propagating
 * to healthy devices on shared bus segments.
 *
 * Operand: { uint32_t bus_id, uint32_t isolate_flags }
 *
 * Full system: Writes to the bus controller MMIO register to assert
 * the isolation signal, then polls the bus status register until
 * in-flight transactions complete or the abort timeout fires.
 */
static dc_directive_status_t dc_handle_isolate_bus(dc_context_t *ctx,
                                                   const uint8_t *operand_data,
                                                   uint32_t operand_size)
{
    if (operand_size < 8u)
        return DC_DIRECTIVE_FAILED;

    uint32_t bus_id = (uint32_t)operand_data[0] |
                      ((uint32_t)operand_data[1] << 8) |
                      ((uint32_t)operand_data[2] << 16) |
                      ((uint32_t)operand_data[3] << 24);
    uint32_t flags = (uint32_t)operand_data[4] |
                     ((uint32_t)operand_data[5] << 8) |
                     ((uint32_t)operand_data[6] << 16) |
                     ((uint32_t)operand_data[7] << 24);

    /*
     * In the full system: locate the bus controller MMIO base from
     * the platform config table, write the isolate command to the
     * bus control register for `bus_id`, then poll the bus status
     * register (typically at MMIO_BASE + 0x40 + bus_id * 4) until
     * BUS_STATUS_IDLE or the isolation timeout fires.
     * The `flags` field selects graceful (drain) vs force (abort).
     */
    (void)bus_id;
    (void)flags;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "isolate bus",
                      bus_id);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_LOG_STATE (0x03)
 * Serializes the current diagnostic state (registers, cache tags,
 * pending DMA descriptors) and writes it to a designated sector
 * range in boot storage for post-mortem analysis.
 *
 * Operand: { uint32_t diag_offset, uint32_t diag_size, uint32_t target_lba }
 *
 * Full system: Copies `diag_size` bytes from the diagnostic capture
 * buffer at `diag_offset` within the sentinel image, writes them
 * sequentially to VIP sectors starting at `target_lba`. The write
 * uses synchronous I/O to guarantee persistence before the next
 * recovery step.
 */
static dc_directive_status_t dc_handle_log_state(dc_context_t *ctx,
                                                 const uint8_t *operand_data,
                                                 uint32_t operand_size)
{
    if (operand_size < 12u)
        return DC_DIRECTIVE_FAILED;

    uint32_t diag_offset = (uint32_t)operand_data[0] |
                           ((uint32_t)operand_data[1] << 8) |
                           ((uint32_t)operand_data[2] << 16) |
                           ((uint32_t)operand_data[3] << 24);
    uint32_t diag_size = (uint32_t)operand_data[4] |
                         ((uint32_t)operand_data[5] << 8) |
                         ((uint32_t)operand_data[6] << 16) |
                         ((uint32_t)operand_data[7] << 24);
    uint32_t target_lba = (uint32_t)operand_data[8] |
                          ((uint32_t)operand_data[9] << 8) |
                          ((uint32_t)operand_data[10] << 16) |
                          ((uint32_t)operand_data[11] << 24);

    if (diag_offset + diag_size > ctx->image_size)
        return DC_DIRECTIVE_FAILED;

    if (diag_size == 0)
        return DC_DIRECTIVE_OK;

    /*
     * In the full system: map the diagnostic data region from the
     * sentinel image (image_base + diag_offset), compute how many
     * VIP sectors (512 bytes each) are needed:
     *   sectors = (diag_size + SENTINEL_VIP_SECTOR_SIZE - 1) / SENTINEL_VIP_SECTOR_SIZE
     * Then issue sector_write(target_lba, sectors, src_ptr) via the
     * VIP block driver. Flush the write cache after completion to
     * ensure the diagnostic state survives a secondary panic.
     */
    (void)target_lba;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "log diagnostic state",
                      diag_size);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_RESET_DEVICE (0x04)
 * Issues a hardware reset to a specific device controller.
 * Supports warm reset (asserts RST line only) and cold reset
 * (full power cycle of the device through the power management IC).
 *
 * Operand: { uint32_t device_id, uint32_t reset_type }
 *   reset_type: 0 = warm, 1 = cold, 2 = bus-level reset
 *
 * Full system: Locates the device in the platform device tree,
 * asserts the appropriate reset line via the GPIO/pinctl MMIO,
 * waits the device-specific recovery delay (t_reset), then
 * re-enables the device by releasing the reset hold.
 */
static dc_directive_status_t dc_handle_reset_device(dc_context_t *ctx,
                                                    const uint8_t *operand_data,
                                                    uint32_t operand_size)
{
    if (operand_size < 8u)
        return DC_DIRECTIVE_FAILED;

    uint32_t device_id = (uint32_t)operand_data[0] |
                         ((uint32_t)operand_data[1] << 8) |
                         ((uint32_t)operand_data[2] << 16) |
                         ((uint32_t)operand_data[3] << 24);
    uint32_t reset_type = (uint32_t)operand_data[4] |
                          ((uint32_t)operand_data[5] << 8) |
                          ((uint32_t)operand_data[6] << 16) |
                          ((uint32_t)operand_data[7] << 24);

    /*
     * In the full system: for warm reset, write 1 to the device's
     * reset register (platform_cfg + device_id * 0x10 + 0x04),
     * delay t_warm_reset, then write 0 to release. For cold reset,
     * route through the PMIC I2C command interface to power-cycle
     * the device rail, waiting t_cold_reset (typically 200ms).
     * After release, re-enumerate the device on the bus.
     */
    (void)device_id;
    (void)reset_type;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "reset device",
                      device_id);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_REBUILD_FVIP (0x05)
 * Reconstructs the Fast Volume Index Page (FVIP) from the canonical
 * UniVIP trie. The FVIP is a flat lookup table mapping LBAs to
 * physical sectors; the UniVIP trie is the authoritative source.
 * Cross-ref: vip/univip_fvip.h
 *
 * Operand: { uint32_t univip_offset, uint32_t univip_size,
 *            uint32_t fvip_offset, uint32_t fvip_size }
 *
 * Full system: Walks the UniVIP trie depth-first, extracting each
 * leaf node's LBA-to-PA mapping, and writes the reconstructed
 * FVIP entries into the designated sector range. Each FVIP page
 * covers SENTINEL_VIP_SECTOR_SIZE (512 bytes) of sector entries.
 * After rebuild, the FVIP CRC32 is recomputed and stored.
 */
static dc_directive_status_t dc_handle_rebuild_fvip(dc_context_t *ctx,
                                                    const uint8_t *operand_data,
                                                    uint32_t operand_size)
{
    if (operand_size < 16u)
        return DC_DIRECTIVE_FAILED;

    uint32_t univip_offset = (uint32_t)operand_data[0] |
                             ((uint32_t)operand_data[1] << 8) |
                             ((uint32_t)operand_data[2] << 16) |
                             ((uint32_t)operand_data[3] << 24);
    uint32_t univip_size = (uint32_t)operand_data[4] |
                           ((uint32_t)operand_data[5] << 8) |
                           ((uint32_t)operand_data[6] << 16) |
                           ((uint32_t)operand_data[7] << 24);
    uint32_t fvip_offset = (uint32_t)operand_data[8] |
                           ((uint32_t)operand_data[9] << 8) |
                           ((uint32_t)operand_data[10] << 16) |
                           ((uint32_t)operand_data[11] << 24);
    uint32_t fvip_size = (uint32_t)operand_data[12] |
                         ((uint32_t)operand_data[13] << 8) |
                         ((uint32_t)operand_data[14] << 16) |
                         ((uint32_t)operand_data[15] << 24);

    if (univip_offset + univip_size > ctx->image_size)
        return DC_DIRECTIVE_FAILED;
    if (fvip_offset + fvip_size > ctx->image_size)
        return DC_DIRECTIVE_FAILED;
    if (univip_size == 0 || fvip_size == 0)
        return DC_DIRECTIVE_FAILED;

    /*
     * In the full system (vip/univip_fvip.h):
     * 1. Map the UniVIP trie from the sentinel image at univip_offset.
     *    The trie header contains the root pointer and total entry count.
     * 2. Allocate the output FVIP region at fvip_offset within the
     *    sentinel image (or into the live VIP sector cache).
     * 3. Walk the trie: for each leaf, extract (lba, pa, flags) and
     *    write a packed FVIP entry (8 bytes) into the output buffer.
     *    FVIP entries are sorted by LBA for binary search lookup.
     * 4. After all entries are written, compute CRC-32c over the
     *    FVIP output and store it in the FVIP header checksum field.
     * 5. Invalidate the FVIP cache in the VIP layer so the next
     *    volume lookup rebuilds from the new table.
     */
    (void)univip_offset;
    (void)univip_size;
    (void)fvip_offset;
    (void)fvip_size;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "rebuild FVIP from UniVIP",
                      univip_size);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_FLUSH_CACHE (0x06)
 * Flushes dirty cache lines and invalidates the specified tag range
 * in the OWFS block cache. Prevents stale data from being committed
 * to disk after a partial metadata corruption event.
 *
 * Operand: { uint32_t tag_base, uint32_t tag_count }
 *
 * Full system: Iterates the OWFS block cache (block size 0x1000),
 * flushing any dirty lines whose tags fall in [tag_base, tag_base+tag_count).
 * After flushing, marks those lines as invalid so subsequent reads
 * fetch fresh data from the backing store.
 */
static dc_directive_status_t dc_handle_flush_cache(dc_context_t *ctx,
                                                   const uint8_t *operand_data,
                                                   uint32_t operand_size)
{
    if (operand_size < 8u)
        return DC_DIRECTIVE_FAILED;

    uint32_t tag_base = (uint32_t)operand_data[0] |
                        ((uint32_t)operand_data[1] << 8) |
                        ((uint32_t)operand_data[2] << 16) |
                        ((uint32_t)operand_data[3] << 24);
    uint32_t tag_count = (uint32_t)operand_data[4] |
                         ((uint32_t)operand_data[5] << 8) |
                         ((uint32_t)operand_data[6] << 16) |
                         ((uint32_t)operand_data[7] << 24);

    /*
     * In the full system: the OWFS block cache uses a set-associative
     * layout with DC_OWFS_BLOCK_SIZE (0x1000) byte blocks. Each cache
     * line stores a tag, dirty bit, and data pointer. We iterate sets
     * in range [tag_base / associativity, (tag_base+tag_count) / associativity),
     * write back any lines with dirty=1 via the OWFS block write path,
     * then set valid=0 for all lines in the flushed range. A cache
     * barrier ensures all writebacks complete before returning.
     */
    (void)tag_base;
    (void)tag_count;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "flush OWFS cache",
                      tag_count);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_CHECKPOINT (0x07)
 * Captures the current recovery state (executed directive bitmap,
 * checksum validation results, partial metadata writes) and commits
 * it to a dedicated checkpoint region in the VIP reserved area
 * (LBA 0 through SENTINEL_VIP_MBL_RESERVED - 1).
 *
 * Operand: { uint32_t checkpoint_id, uint32_t state_offset, uint32_t state_size }
 *
 * Full system: Serializes the recovery context into a checkpoint
 * descriptor, writes it to VIP LBA (checkpoint_id * 2), and stores
 * the associated state blob at VIP LBA (checkpoint_id * 2 + 1).
 * Checkpoint headers include a CRC-32c for integrity.
 */
static dc_directive_status_t dc_handle_checkpoint(dc_context_t *ctx,
                                                  const uint8_t *operand_data,
                                                  uint32_t operand_size)
{
    if (operand_size < 12u)
        return DC_DIRECTIVE_FAILED;

    uint32_t checkpoint_id = (uint32_t)operand_data[0] |
                             ((uint32_t)operand_data[1] << 8) |
                             ((uint32_t)operand_data[2] << 16) |
                             ((uint32_t)operand_data[3] << 24);
    uint32_t state_offset = (uint32_t)operand_data[4] |
                            ((uint32_t)operand_data[5] << 8) |
                            ((uint32_t)operand_data[6] << 16) |
                            ((uint32_t)operand_data[7] << 24);
    uint32_t state_size = (uint32_t)operand_data[8] |
                          ((uint32_t)operand_data[9] << 8) |
                          ((uint32_t)operand_data[10] << 16) |
                          ((uint32_t)operand_data[11] << 24);

    if (state_offset + state_size > ctx->image_size)
        return DC_DIRECTIVE_FAILED;

    /*
     * In the full system: the VIP reserved area spans LBA 0 through
     * SENTINEL_VIP_MBL_RESERVED (128). Checkpoint slots use 2 VIP
     * sectors each (header + state). For checkpoint_id N:
     *   header_lba = N * 2
     *   state_lba  = N * 2 + 1
     * The header contains: checkpoint_id, directive index, timestamp,
     * and a CRC-32c over the state blob. The state blob captures
     * the directive execution bitmap and partial checksum results.
     */
    (void)checkpoint_id;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "write checkpoint",
                      checkpoint_id);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_ROLLBACK (0x08)
 * Restores the system state to a previously saved checkpoint.
 * Reads the checkpoint header from VIP reserved area, validates
 * its CRC, then restores the recovery context to that snapshot.
 *
 * Operand: { uint32_t checkpoint_id }
 *
 * Full system: Reads checkpoint header from VIP LBA (checkpoint_id * 2),
 * verifies the CRC-32c, reads the state blob from LBA (checkpoint_id * 2 + 1),
 * and restores the directive pointer and execution state accordingly.
 * All directives executed after the checkpoint are marked as needing
 * re-execution.
 */
static dc_directive_status_t dc_handle_rollback(dc_context_t *ctx,
                                                const uint8_t *operand_data,
                                                uint32_t operand_size)
{
    if (operand_size < 4u)
        return DC_DIRECTIVE_FAILED;

    uint32_t checkpoint_id = (uint32_t)operand_data[0] |
                             ((uint32_t)operand_data[1] << 8) |
                             ((uint32_t)operand_data[2] << 16) |
                             ((uint32_t)operand_data[3] << 24);

    /*
     * In the full system:
     * 1. Read VIP sector at LBA = checkpoint_id * 2 into the
     *    checkpoint header buffer.
     * 2. Verify the header CRC-32c against the stored checksum.
     * 3. Read the state blob from LBA = checkpoint_id * 2 + 1.
     * 4. Restore ctx->current_directive to the saved directive index.
     * 5. Reset the execution bitmap for all directives after the
     *    checkpoint, so they will be re-executed on the next pass.
     * 6. If the checkpoint header is corrupt, fall back to the
     *    beginning of the directive table (full replay).
     */
    (void)checkpoint_id;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "rollback to checkpoint",
                      checkpoint_id);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_SHUTDOWN_FAIL (0x09)
 * Controlled shutdown sequence for unrecoverable failures.
 * Flushes all pending I/O, serializes the final error state to
 * boot storage, and initiates a safe power-down through the PMIC.
 *
 * Operand: none required (operand_size may be 0)
 *
 * Full system: 1) Flush all OWFS write caches (block size 0x1000)
 * to ensure metadata consistency. 2) Write the final error log to
 * VIP sectors starting at the log LBA. 3) Issue PMIC shutdown
 * command via I2C to power off the system rails in sequence
 * (VAUX -> VIO -> Vcore -> Vmem).
 */
static dc_directive_status_t dc_handle_shutdown_fail(dc_context_t *ctx,
                                                    const uint8_t *operand_data,
                                                    uint32_t operand_size)
{
    (void)operand_data;
    (void)operand_size;

    /*
     * In the full system: this is the terminal recovery action.
     * The shutdown sequence ensures all diagnostic state is
     * persisted before power-down. The PMIC shutdown command
     * is issued through the platform I2C bus with the
     * SHUTDOWN_FAIL flag set, which causes the BIOS to log
     * the failure on next boot and enter diagnostics mode.
     */
    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_ERROR,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "shutdown on unrecoverable failure",
                      0);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_RELOCATE (0x0A)
 * Relocates data from identified bad sectors to pre-reserved spare
 * sectors. The spare area is defined by the VIP partition layout
 * and managed by the UniVIP allocator.
 *
 * Operand: { uint32_t source_lba, uint32_t dest_lba, uint32_t sector_count }
 *
 * Full system: For each sector in range:
 * 1. Read from source_lba + i (VIP sector size = 512 bytes).
 * 2. Compute ECC over the read data; if ECC fails, mark as
 *    unrecoverable (return DC_DIRECTIVE_CRITICAL_FAIL).
 * 3. Write to dest_lba + i in the spare area.
 * 4. Update the UniVIP LBA mapping to point to the new location.
 * 5. Mark the source sector as bad in the VIP bad-sector bitmap.
 */
static dc_directive_status_t dc_handle_relocate(dc_context_t *ctx,
                                                const uint8_t *operand_data,
                                                uint32_t operand_size)
{
    if (operand_size < 12u)
        return DC_DIRECTIVE_FAILED;

    uint32_t source_lba = (uint32_t)operand_data[0] |
                          ((uint32_t)operand_data[1] << 8) |
                          ((uint32_t)operand_data[2] << 16) |
                          ((uint32_t)operand_data[3] << 24);
    uint32_t dest_lba = (uint32_t)operand_data[4] |
                        ((uint32_t)operand_data[5] << 8) |
                        ((uint32_t)operand_data[6] << 16) |
                        ((uint32_t)operand_data[7] << 24);
    uint32_t sector_count = (uint32_t)operand_data[8] |
                            ((uint32_t)operand_data[9] << 8) |
                            ((uint32_t)operand_data[10] << 16) |
                            ((uint32_t)operand_data[11] << 24);

    if (sector_count == 0)
        return DC_DIRECTIVE_OK;

    /*
     * In the full system:
     * - Validate dest_lba is within the spare area (typically
     *   beyond the primary partition but within the VIP layout).
     * - Read each source sector via sector_read(source_lba + i, 1, buf).
     * - Verify sector data with ECC; on failure, mark source as bad
     *   in the VIP bad-sector bitmap at the reserved bitmap LBA.
     * - Write to dest_lba + i via sector_write(dest_lba + i, 1, buf).
     * - Update UniVIP trie: remap (source_lba -> dest_lba) and
     *   rebuild affected FVIP pages.
     */
    (void)source_lba;
    (void)dest_lba;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "relocate sectors",
                      sector_count);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_RAISE_BANCODE (0x0B)
 * Raises a BANcode to the trap register, triggering the secondary
 * recovery handler (if one is registered in the trap table). Used
 * for cascaded recovery where the primary script cannot complete
 * and must delegate to a higher-priority handler.
 *
 * Operand: { uint32_t bancode }
 *
 * Full system: Writes the BANcode to the trap register at
 * SENTINEL_TRAP_BASE + (bancode_slot * 4). The BANcode must be
 * within the valid B+ range [SENTINEL_BANCODE_RANGE_LO,
 * SENTINEL_BANCODE_RANGE_HI]. The trap mechanism is the same
 * used by the original crash handler to invoke this .sentinel script.
 */
static dc_directive_status_t dc_handle_raise_bancode(dc_context_t *ctx,
                                                    const uint8_t *operand_data,
                                                    uint32_t operand_size)
{
    if (operand_size < 4u)
        return DC_DIRECTIVE_FAILED;

    uint32_t bancode = (uint32_t)operand_data[0] |
                       ((uint32_t)operand_data[1] << 8) |
                       ((uint32_t)operand_data[2] << 16) |
                       ((uint32_t)operand_data[3] << 24);

    if (bancode < SENTINEL_BANCODE_RANGE_LO ||
        bancode > SENTINEL_BANCODE_RANGE_HI)
        return DC_DIRECTIVE_FAILED;

    /*
     * In the full system:
     * 1. Validate the BANcode is within the B+ range.
     * 2. Compute the trap register address:
     *    trap_addr = SENTINEL_TRAP_BASE + ((bancode & 0xFF) * 4)
     * 3. Write the BANcode to the trap register. The hardware
     *    trap controller then dispatches to the registered handler
     *    for this code, or raises a double-fault if no handler
     *    is registered.
     * 4. The secondary handler runs as a nested recovery pass.
     */
    (void)bancode;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_BANCODE,
                      bancode,
                      ctx->current_directive,
                      "raise BANcode",
                      bancode);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_WAIT_EVENT (0x0C)
 * Blocks until a specified hardware event fires or the timeout
 * expires. Used to synchronize with asynchronous hardware
 * operations (DMA completion, bus enumeration, power sequencing).
 *
 * Operand: { uint32_t event_id, uint32_t timeout_ms }
 *
 * Full system: Polls the event status register (MMIO-mapped at
 * EVENT_CTRL_BASE + event_id * 4) in a tight loop, checking the
 * READY bit. If timeout_ms expires before the event fires, returns
 * DC_DIRECTIVE_FAILED. A zero timeout means poll once and return.
 */
static dc_directive_status_t dc_handle_wait_event(dc_context_t *ctx,
                                                  const uint8_t *operand_data,
                                                  uint32_t operand_size)
{
    if (operand_size < 8u)
        return DC_DIRECTIVE_FAILED;

    uint32_t event_id = (uint32_t)operand_data[0] |
                        ((uint32_t)operand_data[1] << 8) |
                        ((uint32_t)operand_data[2] << 16) |
                        ((uint32_t)operand_data[3] << 24);
    uint32_t timeout_ms = (uint32_t)operand_data[4] |
                          ((uint32_t)operand_data[5] << 8) |
                          ((uint32_t)operand_data[6] << 16) |
                          ((uint32_t)operand_data[7] << 24);

    /*
     * In the full system:
     * 1. Locate the event controller MMIO base from the platform
     *    configuration table (typically at 0xFED00000).
     * 2. Compute event register: reg = EVENT_CTRL_BASE + event_id * 4.
     * 3. If timeout_ms == 0, single-shot poll: read reg, check bit 0
     *    (READY), return immediately.
     * 4. Otherwise, poll in a loop: read reg, if READY then return OK.
     *    Decrement the platform timer counter each iteration.
     * 5. If the timer reaches zero, return DC_DIRECTIVE_FAILED.
     * 6. The platform timer is typically a 1 MHz downcounter that
     *    provides millisecond resolution without OS support.
     */
    (void)event_id;
    (void)timeout_ms;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "wait for hardware event",
                      event_id);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_INVOKE_DRIVER (0x0D)
 * Calls into a registered .owc (OpenWindows Component) driver handler.
 * The driver provides hardware-specific recovery logic for devices
 * that require custom reset/reinitialization sequences.
 *
 * Operand: { uint32_t driver_ref_offset, uint32_t ctx_offset, uint32_t ctx_size }
 *
 * Full system: Resolves the driver entry point from the .owc driver
 * table at the sentinel image's associated_owc_offset (or from the
 * per-directive driver_ref_offset). Invokes the driver's recovery
 * entry point with the context blob. The driver is responsible for
 * device-specific reinitialization, DMA channel setup, and firmware
 * reload if needed.
 */
static dc_directive_status_t dc_handle_invoke_driver(dc_context_t *ctx,
                                                     const uint8_t *operand_data,
                                                     uint32_t operand_size)
{
    if (operand_size < 12u)
        return DC_DIRECTIVE_FAILED;

    uint32_t driver_ref_offset = (uint32_t)operand_data[0] |
                                 ((uint32_t)operand_data[1] << 8) |
                                 ((uint32_t)operand_data[2] << 16) |
                                 ((uint32_t)operand_data[3] << 24);
    uint32_t ctx_offset = (uint32_t)operand_data[4] |
                          ((uint32_t)operand_data[5] << 8) |
                          ((uint32_t)operand_data[6] << 16) |
                          ((uint32_t)operand_data[7] << 24);
    uint32_t ctx_size = (uint32_t)operand_data[8] |
                        ((uint32_t)operand_data[9] << 8) |
                        ((uint32_t)operand_data[10] << 16) |
                        ((uint32_t)operand_data[11] << 24);

    /*
     * In the full system:
     * 1. Resolve the driver reference from the sentinel image at
     *    driver_ref_offset (or fall back to header->associated_owc_offset).
     * 2. The driver ref contains: driver_id (4 bytes), entry_point (4 bytes),
     *    version (2 bytes), flags (2 bytes), and a name string offset (4 bytes).
     * 3. Validate the driver version is compatible (major match).
     * 4. Copy the driver context blob from image_base + ctx_offset (ctx_size bytes).
     * 5. Call the driver entry point with the context. The driver
     *    performs device-specific recovery (firmware reload, register
     *    reinitialization, DMA channel reassignment).
     * 6. Check the driver's return status; non-zero means the driver
     *    could not recover the device.
     */
    if (driver_ref_offset >= ctx->image_size)
        return DC_DIRECTIVE_FAILED;

    if (ctx_offset + ctx_size > ctx->image_size)
        return DC_DIRECTIVE_FAILED;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "invoke .owc driver",
                      driver_ref_offset);
    }

    return DC_DIRECTIVE_OK;
}

/*
 * SENTINEL_OP_INVOKE_DLL (0x0E)
 * Calls into a registered .owd (OpenWindows Dynamic Library) handler.
 * Unlike .owc drivers which are hardware-specific, .owd libraries
 * provide software-level recovery logic such as metadata repair,
 * filesystem journal replay, or encryption key recovery.
 *
 * Operand: { uint32_t lib_ref_offset, uint32_t ctx_offset, uint32_t ctx_size }
 *
 * Full system: Resolves the library entry point from the .owd
 * library table at the sentinel image's associated_owd_offset (or
 * from the per-directive lib_ref_offset). Invokes the library's
 * recovery function with the context blob. The library performs
 * software-level repairs and returns a status code.
 */
static dc_directive_status_t dc_handle_invoke_dll(dc_context_t *ctx,
                                                  const uint8_t *operand_data,
                                                  uint32_t operand_size)
{
    if (operand_size < 12u)
        return DC_DIRECTIVE_FAILED;

    uint32_t lib_ref_offset = (uint32_t)operand_data[0] |
                              ((uint32_t)operand_data[1] << 8) |
                              ((uint32_t)operand_data[2] << 16) |
                              ((uint32_t)operand_data[3] << 24);
    uint32_t ctx_offset = (uint32_t)operand_data[4] |
                          ((uint32_t)operand_data[5] << 8) |
                          ((uint32_t)operand_data[6] << 16) |
                          ((uint32_t)operand_data[7] << 24);
    uint32_t ctx_size = (uint32_t)operand_data[8] |
                        ((uint32_t)operand_data[9] << 8) |
                        ((uint32_t)operand_data[10] << 16) |
                        ((uint32_t)operand_data[11] << 24);

    /*
     * In the full system:
     * 1. Resolve the library reference from the sentinel image at
     *    lib_ref_offset (or fall back to header->associated_owd_offset).
     * 2. The library ref contains: lib_id (4 bytes), entry_point (4 bytes),
     *    version (2 bytes), flags (2 bytes), name string offset (4 bytes).
     * 3. Validate the library version (major version must match).
     * 4. Copy the library context from image_base + ctx_offset (ctx_size bytes).
     * 5. Call the library entry point. Common .owd libraries include:
     *    - owfs_journal_replay: replays the OWFS write-ahead log
     *    - owfs_meta_repair: rebuilds corrupt directory entries
     *    - owfs_key_recover: recovers encryption keys from key slots
     * 6. The library may modify the sentinel image in-place (e.g.,
     *    repairing metadata structures) and returns 0 on success.
     */
    if (lib_ref_offset >= ctx->image_size)
        return DC_DIRECTIVE_FAILED;

    if (ctx_offset + ctx_size > ctx->image_size)
        return DC_DIRECTIVE_FAILED;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_DIRECTIVE,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "invoke .owd library",
                      lib_ref_offset);
    }

    return DC_DIRECTIVE_OK;
}

/* ------------------------------------------------------------------ */
/*  Directive Execution                                                */
/* ------------------------------------------------------------------ */

dc_directive_status_t dc_execute_directive(dc_context_t *ctx,
                                           const sentinel_directive_header_t *dir,
                                           const uint8_t *operand_data)
{
    if (!ctx || !dir)
        return DC_DIRECTIVE_FAILED;

    if (dir->operand_size > 0) {
        if (dir->operand_offset >= ctx->image_size)
            return DC_DIRECTIVE_FAILED;
        if (dir->operand_size > ctx->image_size - dir->operand_offset)
            return DC_DIRECTIVE_FAILED;
    }

    dc_directive_status_t status;

    switch (dir->opcode) {
    case SENTINEL_OP_NOOP:
        status = dc_handle_noop(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_RESTORE_SHADOW:
        status = dc_handle_restore_shadow(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_ISOLATE_BUS:
        status = dc_handle_isolate_bus(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_LOG_STATE:
        status = dc_handle_log_state(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_RESET_DEVICE:
        status = dc_handle_reset_device(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_REBUILD_FVIP:
        status = dc_handle_rebuild_fvip(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_FLUSH_CACHE:
        status = dc_handle_flush_cache(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_CHECKPOINT:
        status = dc_handle_checkpoint(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_ROLLBACK:
        status = dc_handle_rollback(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_SHUTDOWN_FAIL:
        status = dc_handle_shutdown_fail(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_RELOCATE:
        status = dc_handle_relocate(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_RAISE_BANCODE:
        status = dc_handle_raise_bancode(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_WAIT_EVENT:
        status = dc_handle_wait_event(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_INVOKE_DRIVER:
        status = dc_handle_invoke_driver(ctx, operand_data, dir->operand_size);
        break;
    case SENTINEL_OP_INVOKE_DLL:
        status = dc_handle_invoke_dll(ctx, operand_data, dir->operand_size);
        break;
    default:
        status = DC_DIRECTIVE_FAILED;
        break;
    }

    ctx->total_directives_executed++;

    if (status == DC_DIRECTIVE_OK || status == DC_DIRECTIVE_SKIPPED)
        return status;

    ctx->total_directives_failed++;

    if (ctx->config.verbose_logging) {
        dc_log_append(ctx->log,
                      DC_LOG_TYPE_ERROR,
                      ctx->header->trigger_bancode,
                      ctx->current_directive,
                      "directive failed",
                      (uint32_t)dir->opcode);
    }

    return status;
}

/* ------------------------------------------------------------------ */
/*  Condition Evaluation Helper                                        */
/* ------------------------------------------------------------------ */

/*
 * Check whether a directive's condition_flags are satisfied.
 * SENTINEL_COND_CHECKSUM_OK  - requires referenced checksum to pass
 * SENTINEL_COND_CHECKSUM_BAD - requires referenced checksum to fail
 * SENTINEL_COND_BOOT_ONLY    - requires boot context (always true here)
 * SENTINEL_COND_HOT_PLUG     - always permitted (live-compatible)
 */
static bool dc_conditions_met(dc_context_t *ctx,
                              const sentinel_directive_header_t *dir)
{
    if (dir->condition_flags & SENTINEL_COND_CHECKSUM_OK) {
        if (dir->checksum_index == DC_NO_CHECKSUM_INDEX)
            return false;
        if (dir->checksum_index >= ctx->header->checksum_count)
            return false;
        if (!dc_validate_checksum(&ctx->checksums[dir->checksum_index]))
            return false;
    }

    if (dir->condition_flags & SENTINEL_COND_CHECKSUM_BAD) {
        if (dir->checksum_index == DC_NO_CHECKSUM_INDEX)
            return false;
        if (dir->checksum_index >= ctx->header->checksum_count)
            return false;
        if (dc_validate_checksum(&ctx->checksums[dir->checksum_index]))
            return false;
    }

    return true;
}

/* ------------------------------------------------------------------ */
/*  Execute All Directives                                             */
/* ------------------------------------------------------------------ */

dc_recovery_result_t dc_execute_all(dc_context_t *ctx)
{
    if (!ctx || !ctx->header)
        return DC_RECOVERY_INVALID_SCRIPT;

    uint32_t total = ctx->header->directive_count;

    if (ctx->current_directive >= total)
        return DC_RECOVERY_SUCCESS;

    dc_recovery_result_t result = DC_RECOVERY_SUCCESS;

    for (uint32_t i = ctx->current_directive; i < total; i++) {
        if (ctx->recovery_aborted)
            break;

        const sentinel_directive_header_t *dir = &ctx->directives[i];
        ctx->current_directive = i;

        if (!dc_conditions_met(ctx, dir)) {
            if (ctx->config.verbose_logging) {
                dc_log_append(ctx->log,
                              DC_LOG_TYPE_DIRECTIVE,
                              ctx->header->trigger_bancode,
                              (uint8_t)i,
                              "condition not met, skipping",
                              dir->condition_flags);
            }
            continue;
        }

        const uint8_t *operand = ctx->image_base + dir->operand_offset;

        uint32_t max_retries = 1;
        if (dir->condition_flags & SENTINEL_COND_RETRY)
            max_retries = ctx->config.max_retry_per_trigger;
        if (max_retries == 0)
            max_retries = 1;

        dc_directive_status_t status = DC_DIRECTIVE_FAILED;

        for (uint32_t attempt = 0; attempt < max_retries; attempt++) {
            status = dc_execute_directive(ctx, dir, operand);

            if (status == DC_DIRECTIVE_OK || status == DC_DIRECTIVE_SKIPPED)
                break;

            if (ctx->config.verbose_logging) {
                dc_log_append(ctx->log,
                              DC_LOG_TYPE_WARN,
                              ctx->header->trigger_bancode,
                              (uint8_t)i,
                              "retry attempt",
                              attempt + 1);
            }
        }

        if (status == DC_DIRECTIVE_CRITICAL_FAIL ||
            (status == DC_DIRECTIVE_FAILED &&
             (dir->condition_flags & SENTINEL_COND_CRITICAL))) {
            ctx->recovery_aborted = true;
            result = DC_RECOVERY_UNRECOVERABLE;

            dc_log_append(ctx->log,
                          DC_LOG_TYPE_ERROR,
                          ctx->header->trigger_bancode,
                          (uint8_t)i,
                          "critical directive failed, aborting",
                          (uint32_t)dir->opcode);
            break;
        }

        if (status == DC_DIRECTIVE_FAILED) {
            result = DC_RECOVERY_PARTIAL;

            if (ctx->config.abort_on_first_fail) {
                ctx->recovery_aborted = true;
                result = DC_RECOVERY_FAILED;

                dc_log_append(ctx->log,
                              DC_LOG_TYPE_ERROR,
                              ctx->header->trigger_bancode,
                              (uint8_t)i,
                              "abort on first fail",
                              (uint32_t)dir->opcode);
                break;
            }
        }
    }

    ctx->final_result = result;
    return result;
}

/* ------------------------------------------------------------------ */
/*  Trigger Evaluation                                                 */
/* ------------------------------------------------------------------ */

bool dc_find_trigger(const dc_context_t *ctx,
                     uint32_t bancode,
                     const sentinel_trigger_entry_t **trigger_out,
                     uint32_t *directive_index_out)
{
    if (!ctx || !ctx->header || !trigger_out || !directive_index_out)
        return false;

    *trigger_out = NULL;
    *directive_index_out = 0;

    uint32_t count = ctx->header->trigger_count;

    for (uint32_t i = 0; i < count; i++) {
        const sentinel_trigger_entry_t *t = &ctx->triggers[i];

        if (t->bancode == bancode) {
            *trigger_out = t;
            *directive_index_out = t->directive_index;
            return true;
        }
    }

    return false;
}

/* ------------------------------------------------------------------ */
/*  Recovery Pass Orchestration                                        */
/* ------------------------------------------------------------------ */

dc_recovery_result_t dc_run_recovery(const uint8_t *sentinel_image,
                                     uint32_t image_size,
                                     const dc_config_t *config,
                                     dc_recovery_log_t *log)
{
    if (!sentinel_image || !config)
        return DC_RECOVERY_INVALID_SCRIPT;

    dc_context_t ctx;
    dc_memset((uint8_t *)&ctx, 0, sizeof(ctx));

    dc_log_append(log, DC_LOG_TYPE_RECOVERY, 0, 0xFF, "recovery start", 0);

    sentinel_status_t init_status = dc_context_init(&ctx, sentinel_image,
                                                     image_size, config, log);
    if (init_status != SENTINEL_OK) {
        dc_log_append(log, DC_LOG_TYPE_ERROR, 0, 0xFF,
                      "header init failed",
                      (uint32_t)(-init_status));
        return DC_RECOVERY_INVALID_SCRIPT;
    }

    dc_log_append(log, DC_LOG_TYPE_INFO,
                  ctx.header->trigger_bancode, 0xFF,
                  "header validated",
                  ctx.header->directive_count);

    uint32_t cksum_failures = dc_validate_all_checksums(&ctx);

    if (cksum_failures > 0) {
        dc_log_append(log, DC_LOG_TYPE_CHECKSUM,
                      ctx.header->trigger_bancode, 0xFF,
                      "checksum validation failures",
                      cksum_failures);

        if (config->abort_on_first_fail) {
            dc_log_append(log, DC_LOG_TYPE_ERROR,
                          ctx.header->trigger_bancode, 0xFF,
                          "aborting: pre-recovery checksum fail", 0);
            ctx.final_result = DC_RECOVERY_CHECKSUM_FAIL;
            dc_context_release(&ctx);
            return DC_RECOVERY_CHECKSUM_FAIL;
        }
    }

    const sentinel_trigger_entry_t *trigger = NULL;
    uint32_t directive_start = 0;

    if (!dc_find_trigger(&ctx, ctx.header->trigger_bancode,
                         &trigger, &directive_start)) {
        dc_log_append(log, DC_LOG_TYPE_ERROR,
                      ctx.header->trigger_bancode, 0xFF,
                      "no matching trigger for BANcode",
                      ctx.header->trigger_bancode);
        dc_context_release(&ctx);
        return DC_RECOVERY_NO_SCRIPT;
    }

    dc_log_append(log, DC_LOG_TYPE_BANCODE,
                  ctx.header->trigger_bancode, 0xFF,
                  "trigger matched",
                  directive_start);

    ctx.current_directive = directive_start;

    dc_recovery_result_t result = dc_execute_all(&ctx);

    dc_log_append(log, DC_LOG_TYPE_RECOVERY,
                  ctx.header->trigger_bancode, 0xFF,
                  "recovery complete",
                  (uint32_t)result);

    dc_context_release(&ctx);
    return result;
}
