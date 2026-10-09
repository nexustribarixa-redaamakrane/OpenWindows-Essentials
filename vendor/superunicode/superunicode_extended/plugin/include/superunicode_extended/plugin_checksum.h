#ifndef SUCS_PLUGIN_CHECKSUM_H
#define SUCS_PLUGIN_CHECKSUM_H

/**
 * Plugin Integrity Checksums (CRC32c + Fletcher-64)
 *
 * Plugin blobs are integrity-checked at BOOT before mounting. The dual
 * checksum scheme (CRC32c Castagnoli + Fletcher-64) matches the integrity
 * model of the OpenWindows Storage suite (libowfs.a / libusfs.a), so the
 * same primitives protect plugin partitions and the filesystems that
 * host them.
 *
 * Rules:
 *   - CRC32c:     polynomial 0x82F63B78 (Castagnoli), reflected, init 0xFFFFFFFF.
 *   - Fletcher-64: 32-bit words, sums modulo 0xFFFFFFFF, init 0xFFFFFFFF.
 *   - Blob checksums are computed over the ENTIRE blob with the header's
 *     crc32c and fletcher64 fields zeroed.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Fletcher-64 Streaming State
 * ============================================================================ */
typedef struct {
    uint32_t sum1;
    uint32_t sum2;
    uint8_t  pending;
    uint8_t  buf[4];
} sucs_fletcher64_state_t;

void     sucs_fletcher64_init(sucs_fletcher64_state_t* state);
void     sucs_fletcher64_update(sucs_fletcher64_state_t* state, const uint8_t* data, size_t len);
uint64_t sucs_fletcher64_final(sucs_fletcher64_state_t* state);

/* ============================================================================
 * Single-Shot Checksums
 * ============================================================================ */
uint32_t sucs_checksum_crc32c(const uint8_t* data, size_t len);
uint64_t sucs_checksum_fletcher64(const uint8_t* data, size_t len);

/* Low-level CRC32c feed (raw running state, NOT XORed at the end). */
uint32_t sucs_checksum_crc32c_update(uint32_t crc, const uint8_t* data, size_t len);

/* ============================================================================
 * Plugin Blob Utilities
 * ============================================================================ */

/* Computes the canonical blob checksums (both checksum fields zeroed).
 * Returns SUCS_PLUGIN_OK on success, or an error status. */
sucs_plugin_status_t sucs_plugin_compute_checksums(const uint8_t* blob,
                                                   size_t blob_size,
                                                   uint32_t* out_crc,
                                                   uint64_t* out_fletcher);

/* Verifies a blob's stored checksums against a fresh computation. */
bool sucs_plugin_blob_verify(const uint8_t* blob, size_t blob_size);

/* Parses the 16-byte little-endian range records out of a blob payload.
 * Returns SUCS_PLUGIN_OK and the range count, or an error status. */
sucs_plugin_status_t sucs_plugin_parse_ranges(const uint8_t* blob,
                                              size_t blob_size,
                                              sucs_plugin_range_t* out,
                                              uint32_t max_ranges,
                                              uint32_t* out_count);

/* Validates that every range is well-formed and extends PAST the base limit.
 * Returns SUCS_PLUGIN_OK, SUCS_PLUGIN_ERR_INVALID_RANGE, or
 * SUCS_PLUGIN_ERR_RANGE_BELOW_BASE. */
sucs_plugin_status_t sucs_plugin_validate_ranges(const sucs_plugin_range_t* ranges,
                                                 uint32_t count);

#ifdef __cplusplus
}
#endif

#endif /* SUCS_PLUGIN_CHECKSUM_H */
