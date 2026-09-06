/**
 * Test Suite: SuperUnicode Plugin Lifecycle (stage -> boot -> mount)
 *
 * Verifies: CRC32c / Fletcher-64 integrity, blob packing + verification,
 * staging (reboot required), boot commit (checksum gate), OWFS-only plugin
 * partition policy, range registration / collision, and codepoint lookup.
 *
 * NOTE: This test file uses host CRT (<stdio.h>, <assert.h>) for verification.
 * The library code under test remains strictly freestanding.
 */

#include <stdio.h>
#include <assert.h>
#include "superunicode_extended/plugin.h"
#include "superunicode_extended/plugin_checksum.h"
#include "superunicode_extended/plugin_stage.h"
#include "superunicode_extended/plugin_boot.h"
#include "superunicode_extended/plugin_partition.h"

/* ============================================================================
 * Helpers
 * ============================================================================ */

/* Builds a complete, checksum-valid plugin blob in `out`. */
static size_t build_blob(const char* id,
                         uint8_t ver_major, uint8_t ver_minor, uint8_t ver_patch,
                         const sucs_plugin_range_t* ranges, uint32_t range_count,
                         const uint8_t* payload, size_t payload_size,
                         uint8_t* out, size_t out_capacity) {
    size_t hdr_size = sizeof(sucs_plugin_blob_header_t);
    size_t ranges_bytes = (size_t)range_count * sizeof(sucs_plugin_range_t);
    size_t total = hdr_size + ranges_bytes + payload_size;
    assert(total <= out_capacity);

    for (size_t i = 0; i < total; ++i) {
        out[i] = 0;
    }

    sucs_plugin_blob_header_t* hdr = (sucs_plugin_blob_header_t*)out;
    hdr->magic = SUCS_PLUGIN_BLOB_MAGIC;
    hdr->blob_version = SUCS_PLUGIN_BLOB_VERSION;
    hdr->ver_major = ver_major;
    hdr->ver_minor = ver_minor;
    hdr->ver_patch = ver_patch;
    for (size_t i = 0; id[i] && i < SUCS_PLUGIN_ID_MAX - 1; ++i) {
        hdr->id[i] = (char)id[i];
    }
    hdr->range_count = range_count;
    hdr->blob_size = (uint32_t)total;

    uint8_t* p = out + hdr_size;
    for (uint32_t r = 0; r < range_count; ++r) {
        for (int b = 0; b < 8; ++b) {
            p[r * 16 + b]     = (uint8_t)(ranges[r].start >> (8 * b));
            p[r * 16 + 8 + b] = (uint8_t)(ranges[r].end >> (8 * b));
        }
    }
    if (payload && payload_size) {
        for (size_t i = 0; i < payload_size; ++i) {
            out[hdr_size + ranges_bytes + i] = payload[i];
        }
    }

    uint32_t crc = 0;
    uint64_t fletcher = 0;
    assert(sucs_plugin_compute_checksums(out, total, &crc, &fletcher) == SUCS_PLUGIN_OK);
    hdr->crc32c = crc;
    hdr->fletcher64 = fletcher;
    return total;
}

static void reset_all(void) {
    sucs_plugin_stage_reset();
    sucs_plugin_boot_reset();
}

/* ============================================================================
 * Test: Checksum primitives (known vectors + determinism)
 * ============================================================================ */
void test_checksums(void) {
    /* CRC32c (Castagnoli) of "123456789" is the canonical check value. */
    uint8_t digits[] = {'1','2','3','4','5','6','7','8','9'};
    assert(sucs_checksum_crc32c(digits, 9) == 0xE3069283UL);

    /* Fletcher-64 determinism + sensitivity. */
    uint8_t a[] = {0x01, 0x02, 0x03, 0x04};
    uint8_t b[] = {0x01, 0x02, 0x03, 0x05};
    uint64_t fa = sucs_checksum_fletcher64(a, 4);
    uint64_t fb = sucs_checksum_fletcher64(b, 4);
    assert(fa != fb);
    assert(fa == sucs_checksum_fletcher64(a, 4));

    printf("[PASS] test_checksums\n");
}

/* ============================================================================
 * Test: Blob packing + verification (clean and tampered)
 * ============================================================================ */
void test_blob_verify(void) {
    uint8_t blob[1024];
    sucs_plugin_range_t ranges[1] = { { 0x80000000ULL, 0x80000FFFULL } };
    uint8_t payload[] = { 'H', 'E', 'L', 'L', 'O' };

    size_t size = build_blob("org.openwindows.hellocp", 1, 0, 0,
                             ranges, 1, payload, sizeof(payload), blob, sizeof(blob));
    assert(size > 0);
    assert(sucs_plugin_blob_verify(blob, size) == true);

    /* Corrupted payload breaks verification. */
    blob[size - 1] ^= 0xFF;
    assert(sucs_plugin_blob_verify(blob, size) == false);

    printf("[PASS] test_blob_verify\n");
}

/* ============================================================================
 * Test: Staging (structural validation + reboot required)
 * ============================================================================ */
void test_stage_install(void) {
    reset_all();
    uint8_t blob[1024];
    sucs_plugin_range_t ranges[1] = { { 0x80000000ULL, 0x80000FFFULL } };
    uint8_t payload[] = { 'D', 'A', 'T', 'A' };
    size_t size = build_blob("org.openwindows.testp", 1, 2, 3,
                             ranges, 1, payload, sizeof(payload), blob, sizeof(blob));

    /* Valid blob -> staged, reboot required. */
    assert(sucs_plugin_stage_install(blob, size) == SUCS_PLUGIN_REBOOT_REQUIRED);
    assert(sucs_plugin_get_pending_count() == 1);
    assert(sucs_plugin_is_reboot_required() == true);

    char id[SUCS_PLUGIN_ID_MAX];
    assert(sucs_plugin_get_pending_id(0, id, sizeof(id)) == true);
    assert(id[0] == 'o');

    /* Duplicate id rejected. */
    assert(sucs_plugin_stage_install(blob, size) == SUCS_PLUGIN_ERR_DUPLICATE_ID);

    /* Bad magic rejected. */
    blob[0] ^= 0xFF;
    assert(sucs_plugin_stage_install(blob, size) == SUCS_PLUGIN_ERR_INVALID_BLOB);
    blob[0] ^= 0xFF;

    /* Range below the base limit rejected (plugins must add codepoints). */
    sucs_plugin_range_t low_range[1] = { { 0x00000041ULL, 0x0000007FULL } };
    uint8_t blob2[1024];
    size_t size2 = build_blob("org.openwindows.low", 1, 0, 0,
                              low_range, 1, payload, sizeof(payload), blob2, sizeof(blob2));
    assert(sucs_plugin_stage_install(blob2, size2) == SUCS_PLUGIN_ERR_RANGE_BELOW_BASE);

    reset_all();
    printf("[PASS] test_stage_install\n");
}

/* ============================================================================
 * Test: Boot commit mounts a valid plugin (OWFS-only partition)
 * ============================================================================ */
void test_boot_commit(void) {
    reset_all();
    uint8_t blob[1024];
    sucs_plugin_range_t ranges[1] = { { 0x80000000ULL, 0x80000FFFULL } };
    uint8_t payload[] = { 'D', 'A', 'T', 'A' };
    size_t size = build_blob("org.openwindows.boot", 1, 0, 0,
                             ranges, 1, payload, sizeof(payload), blob, sizeof(blob));

    assert(sucs_plugin_stage_install(blob, size) == SUCS_PLUGIN_REBOOT_REQUIRED);

    sucs_plugin_boot_config_t cfg;
    assert(sucs_plugin_commit_on_boot(&cfg) == true);
    assert(cfg.staged_count == 1);
    assert(cfg.mounted_count == 1);
    assert(cfg.quarantined_count == 0);
    assert(cfg.reboot_required == false);

    assert(sucs_plugin_get_active_count() == 1);
    assert(sucs_plugin_get_quarantined_count() == 0);
    assert(sucs_plugin_partition_get_mounted_count() == 1);
    assert(sucs_plugin_is_reboot_required() == false);

    /* Registered ranges. */
    assert(sucs_plugin_is_range_registered(0x80000000ULL) == true);
    assert(sucs_plugin_is_range_registered(0x80000FFFULL) == true);
    assert(sucs_plugin_is_range_registered(0x80001000ULL) == false);
    assert(sucs_plugin_is_range_registered(0x7FFFFFFFULL) == false);

    /* Codepoint lookup. */
    const sucs_plugin_t* found = 0;
    assert(sucs_plugin_lookup_codepoint(0x80000041ULL, &found) == true);
    assert(found != 0);
    assert(found->ver_major == 1 && found->ver_minor == 0 && found->ver_patch == 0);
    assert(sucs_plugin_lookup_codepoint(0x80002000ULL, &found) == false);

    reset_all();
    printf("[PASS] test_boot_commit\n");
}

/* ============================================================================
 * Test: Boot-time checksum gate quarantines corrupted plugins
 * ============================================================================ */
void test_boot_quarantine_checksum(void) {
    reset_all();
    uint8_t blob[1024];
    sucs_plugin_range_t ranges[1] = { { 0x80000000ULL, 0x80000FFFULL } };
    uint8_t payload[] = { 'D', 'A', 'T', 'A' };
    size_t size = build_blob("org.openwindows.badsum", 1, 0, 0,
                             ranges, 1, payload, sizeof(payload), blob, sizeof(blob));

    /* Corrupt the stored checksum (structure stays valid; checksum breaks). */
    sucs_plugin_blob_header_t* hdr = (sucs_plugin_blob_header_t*)blob;
    hdr->crc32c ^= 1;

    assert(sucs_plugin_stage_install(blob, size) == SUCS_PLUGIN_REBOOT_REQUIRED);

    sucs_plugin_boot_config_t cfg;
    assert(sucs_plugin_commit_on_boot(&cfg) == false);
    assert(cfg.mounted_count == 0);
    assert(cfg.quarantined_count == 1);

    assert(sucs_plugin_get_active_count() == 0);
    assert(sucs_plugin_get_quarantined_count() == 1);
    assert(sucs_plugin_partition_get_mounted_count() == 0);
    assert(sucs_plugin_is_range_registered(0x80000041ULL) == false);

    reset_all();
    printf("[PASS] test_boot_quarantine_checksum\n");
}

/* ============================================================================
 * Test: Range collision between plugins
 * ============================================================================ */
void test_range_collision(void) {
    reset_all();
    uint8_t blobA[1024], blobB[1024];
    sucs_plugin_range_t rangeA[1] = { { 0x80000000ULL, 0x80000FFFULL } };
    sucs_plugin_range_t rangeB[1] = { { 0x80000100ULL, 0x800001FFULL } };
    uint8_t payload[] = { 'X' };

    size_t sizeA = build_blob("org.openwindows.colla", 1, 0, 0,
                              rangeA, 1, payload, 1, blobA, sizeof(blobA));
    size_t sizeB = build_blob("org.openwindows.collb", 1, 0, 0,
                              rangeB, 1, payload, 1, blobB, sizeof(blobB));

    assert(sucs_plugin_stage_install(blobA, sizeA) == SUCS_PLUGIN_REBOOT_REQUIRED);
    assert(sucs_plugin_stage_install(blobB, sizeB) == SUCS_PLUGIN_REBOOT_REQUIRED);

    sucs_plugin_boot_config_t cfg;
    assert(sucs_plugin_commit_on_boot(&cfg) == true);
    assert(cfg.mounted_count == 1);
    assert(cfg.quarantined_count == 1);

    /* A (first) mounted; B collided and was quarantined. */
    const sucs_plugin_t* found = 0;
    assert(sucs_plugin_lookup_codepoint(0x80000150ULL, &found) == true);
    assert(found->id[0] == 'o'); /* colla won the range */

    reset_all();
    printf("[PASS] test_range_collision\n");
}

/* ============================================================================
 * Test: Partition filesystem policy (OWFS-only plugin partitions)
 * ============================================================================ */
void test_partition_fs_policy(void) {
    reset_all();
    sucs_plugin_partition_t part;
    sucs_plugin_range_t ranges[1] = { { 0x80000000ULL, 0x80000FFFULL } };

    /* Permitted SuperUnicode Partition formats (base bugfix/rescue). */
    assert(sucs_plugin_partition_fs_is_valid(SUCS_PARTITION_FS_OWFS) == true);
    assert(sucs_plugin_partition_fs_is_valid(SUCS_PARTITION_FS_USFS) == true);

    /* Plugin partitions MUST be OWFS — USFS is rejected. */
    assert(sucs_plugin_partition_mount(&part, "org.openwindows.owfs",
                                       SUCS_PARTITION_FS_OWFS, ranges, 1) == SUCS_PLUGIN_OK);
    assert(sucs_plugin_partition_mount(&part, "org.openwindows.usfs",
                                       SUCS_PARTITION_FS_USFS, ranges, 1) == SUCS_PLUGIN_ERR_NOT_OWFS);

    /* Mounted plugin partitions are always read-only. */
    assert(part.mounted == true);
    assert(part.read_only == true);
    assert(part.checksum_ok == true);
    assert(sucs_plugin_partition_get_mounted_count() == 1);

    assert(sucs_plugin_partition_unmount(&part) == SUCS_PLUGIN_OK);
    assert(sucs_plugin_partition_get_mounted_count() == 0);
    assert(part.mounted == false);

    reset_all();
    printf("[PASS] test_partition_fs_policy\n");
}

/* ============================================================================
 * Main
 * ============================================================================ */
int main(void) {
    printf("==========================================================\n");
    printf(" RUNNING ALL SUPERUNICODE PLUGIN LIFECYCLE TESTS          \n");
    printf("==========================================================\n");
    test_checksums();
    test_blob_verify();
    test_stage_install();
    test_boot_commit();
    test_boot_quarantine_checksum();
    test_range_collision();
    test_partition_fs_policy();
    printf("==========================================================\n");
    printf(" ALL SUPERUNICODE PLUGIN LIFECYCLE TESTS PASSED!          \n");
    printf("==========================================================\n");
    return 0;
}
