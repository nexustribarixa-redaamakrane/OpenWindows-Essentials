/**
 * Ecosystem Compatibility Test: Modular-Bootloader <-> SuperUnicode
 *
 * Single translation unit co-including:
 *   - sutf's sutf8.h                          (canonical libsutf SUTF-8 codec)
 *   - Modular-Bootloader's vendored <sutf/sucs_types.h> and <sutf/sucs_mode.h>
 *     (guarded SUCS copies that adopt the canonical constants)
 *   - <mbl.h> (boot-config ABI + standalone OWFS mirrors), which pulls in
 *     <bancode/bancode_all.h>
 *
 * Verifies:
 *   1. Guarded-constants coexistence: the MBL sutf copies sit in one TU with
 *      libsutf's headers; every SUCS constant matches the canonical (spec)
 *      value and the BANcode framework constants.
 *   2. Kernel Mode-Switching controller semantics (sucs_mode.c).
 *   3. Boot-config ABI: sucs_kernel_boot_config_t / mbl_boot_config_t field
 *      layout, magic, and fixed-RAM map addresses.
 *   4. OWFS mirrors: the standalone (non-adopted) fallback constants and the
 *      on-disk struct geometry match the canonical storage spec.
 *
 * Build mode: compiles the REAL ../Modular-Bootloader sucs_mode.c when the
 * sibling is checked out next to this workspace, otherwise the vendored port
 * under compat/Modular-Bootloader/.
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

#include "sutf/sucs_types.h"     /* MBL vendored copy (guarded) */
#include "sutf/sucs_mode.h"      /* MBL vendored copy (mode controller) */
#include "mbl.h"                 /* MBL boot config + OWFS mirrors */
#include "sutf8.h"               /* canonical libsutf SUTF-8 codec */

static int g_checks;

#define CHECK(cond) \
    do { ++g_checks; assert(cond); } while (0)

static void test_constant_coexistence(void) {
    /* SUCS contract constants from the MBL vendored sucs_types.h copy must
     * match the canonical spec values libsutf's sutf8.h provides (guarded
     * so both headers coexist in this TU). */
    CHECK(SUCS_INVALID_CODEPOINT == 0x7FFFFFFFUL);
    CHECK(SUCS_MAX_CODEPOINT == 0x7FFFFFFFUL);
    CHECK(SUCS_TRAP_RANGE_MIN == 0x7FFFFFF0UL);
    CHECK(SUCS_TRAP_RANGE_MAX == 0x7FFFFFFEUL);

    /* BANcode framework parity (bancode_all.h pulled in by mbl.h) */
    CHECK(BANCODE_INVALID_CODEPOINT == SUCS_INVALID_CODEPOINT);
    CHECK(BANCODE_KERNEL_TRAP_MIN == SUCS_TRAP_RANGE_MIN);
    CHECK(BANCODE_KERNEL_TRAP_MAX == SUCS_TRAP_RANGE_MAX);

    /* Codepoint validator agreement with the canonical codec */
    CHECK(sucs_is_valid(0x7FFFFFEFUL));
    CHECK(!sucs_is_valid(0x7FFFFFF0UL));
    CHECK(!sucs_is_valid(0x7FFFFFFEUL));
    CHECK(!sucs_is_valid(0x7FFFFFFFUL));
    CHECK(sutf8_codepoint_length(0x7FFFFFEFUL) == 6);
    CHECK(sutf8_codepoint_length(0x7FFFFFF0UL) == 0);
    CHECK(sutf8_codepoint_length(SUCS_INVALID_CODEPOINT) == 0);
    CHECK(sutf8_codepoint_length(0x03FFFFFFUL) == 5);
    CHECK(sutf8_codepoint_length(0x0000FFFFUL) == 3);
}

static void test_mode_controller(void) {
    sucs_kernel_boot_config_t cfg;

    /* Enum literal parity (spec): BASE/EXTENDED and the switch statuses. */
    CHECK(SUCS_MODE_BASE == 0);
    CHECK(SUCS_MODE_EXTENDED == 1);
    CHECK(SUCS_SWITCH_SUCCESS == 0);
    CHECK(SUCS_SWITCH_ERR_INVALID_MODE == 1);
    CHECK(SUCS_SWITCH_ERR_ALREADY_ACTIVE == 2);
    CHECK(SUCS_SWITCH_REBOOT_REQUIRED == 3);

    /* init resets the staged/committed state */
    memset(&cfg, 0, sizeof(cfg));
    sucs_init_boot_config(&cfg, SUCS_MODE_BASE);
    CHECK(cfg.active_mode == SUCS_MODE_BASE);
    CHECK(cfg.pending_mode == SUCS_MODE_BASE);
    CHECK(cfg.reboot_required == false);
    CHECK(cfg.mode_change_count == 0);

    /* An invalid initial mode clamps to BASE */
    sucs_init_boot_config(&cfg, (sucs_kernel_mode_t)99);
    CHECK(cfg.active_mode == SUCS_MODE_BASE);

    /* Default global kernel state starts in BASE with no reboot pending */
    CHECK(sucs_get_active_mode() == SUCS_MODE_BASE);
    CHECK(sucs_get_pending_mode() == SUCS_MODE_BASE);
    CHECK(sucs_is_reboot_required() == false);

    /* Staging a switch never alters the active mode at runtime */
    CHECK(sucs_request_mode_switch(SUCS_MODE_EXTENDED) ==
          SUCS_SWITCH_REBOOT_REQUIRED);
    CHECK(sucs_get_active_mode() == SUCS_MODE_BASE);
    CHECK(sucs_get_pending_mode() == SUCS_MODE_EXTENDED);
    CHECK(sucs_is_reboot_required() == true);

    /* Re-requesting the current active mode while a switch is pending
     * re-stages it (still requires the reboot), not ERR_ALREADY_ACTIVE. */
    CHECK(sucs_request_mode_switch(SUCS_MODE_BASE) == SUCS_SWITCH_REBOOT_REQUIRED);
    CHECK(sucs_get_pending_mode() == SUCS_MODE_BASE);

    /* Unknown modes are rejected outright */
    CHECK(sucs_request_mode_switch((sucs_kernel_mode_t)99) ==
          SUCS_SWITCH_ERR_INVALID_MODE);

    /* Boot-time commit promotes the pending mode and clears the flag */
    memset(&cfg, 0, sizeof(cfg));
    sucs_init_boot_config(&cfg, SUCS_MODE_BASE);
    cfg.pending_mode = SUCS_MODE_EXTENDED;
    cfg.reboot_required = true;
    CHECK(sucs_commit_mode_on_boot(&cfg) == true);
    CHECK(cfg.active_mode == SUCS_MODE_EXTENDED);
    CHECK(cfg.reboot_required == false);
    CHECK(cfg.mode_change_count == 1);
    CHECK(sucs_commit_mode_on_boot(&cfg) == false);
    CHECK(cfg.mode_change_count == 1);

    /* Nothing staged: no commit, active mode untouched */
    memset(&cfg, 0, sizeof(cfg));
    sucs_init_boot_config(&cfg, SUCS_MODE_BASE);
    CHECK(sucs_commit_mode_on_boot(&cfg) == false);
    CHECK(cfg.active_mode == SUCS_MODE_BASE);
}

static void test_boot_config_abi(void) {
    /* sucs_kernel_boot_config_t layout (kernel handoff ABI) */
    CHECK(offsetof(sucs_kernel_boot_config_t, active_mode) == 0);
    CHECK(offsetof(sucs_kernel_boot_config_t, pending_mode) == 4);
    CHECK(offsetof(sucs_kernel_boot_config_t, reboot_required) == 8);
    CHECK(offsetof(sucs_kernel_boot_config_t, mode_change_count) == 12);
    CHECK(sizeof(sucs_kernel_boot_config_t) == 16);

    /* mbl_boot_config_t: fixed-RAM handoff block ('MBL2') */
    CHECK(MBL_MAGIC_BOOTCFG == 0x324C424Du);
    CHECK(offsetof(mbl_boot_config_t, magic) == 0);
    CHECK(offsetof(mbl_boot_config_t, boot_drive) == 4);
    CHECK(offsetof(mbl_boot_config_t, kernel_size) == 8);
    CHECK(offsetof(mbl_boot_config_t, sucs_cfg) == 12);
    CHECK(sizeof(mbl_boot_config_t) == 28);

    /* Fixed RAM map */
    CHECK(MBL_BOOTCONFIG == 0x00000510u);
    CHECK(MBL_FS_BUF == 0x00030000u);
    CHECK(MBL_KERNEL_ADDR == 0x00200000u);
    CHECK(MBL_KERNEL_MAX == 0x01000000u);
}

static void test_owfs_mirrors(void) {
    /* Standalone fallback path: owfs_types.h is NOT on this target's include
     * path, so mbl.h's __has_include adoption does not fire and the spec
     * copies must match the canonical OpenWindows-Storage values. */
    CHECK(OWFS_BLOCK_SIZE == 0x1000u);
    CHECK(OWFS_BLOCK_SHIFT == 12u);
    CHECK(OWFS_PARTITION_LBA == 131200u);
    CHECK(OWFS_SUPERBLOCK_BLOCK == 16u);
    CHECK(OWFS_INODE_SIZE == 0x100u);
    CHECK(OWFS_INODES_PER_BLOCK == 16u);
    CHECK(OWFS_ENTRIES_PER_BLOCK == 16u);
    CHECK(OWFS_NAME_MAX_BYTES == 128u);
    CHECK(OWFS_MAGIC == 0x4F574653UL);   /* 'OWFS' */
    CHECK(OWFS_VERSION_MAJOR == 1u);
    CHECK(OWFS_VERSION_MINOR == 2u);
    CHECK(OWFS_ROOT_INODE == 0u);
    CHECK(OWFS_ENTRY_FILE == 0x01u);
    CHECK(OWFS_ENTRY_CATALOG == 0x02u);
    CHECK(OWFS_ENTRY_DELETED == 0x80u);
    CHECK(OWFS_DIRECT_BLOCKS == 10u);
    CHECK(OWFS_SEC_ENCRYPTED == (1UL << 0));
    CHECK(OWFS_SEC_READONLY == (1UL << 1));
    CHECK(OWFS_SEC_HIDDEN == (1UL << 2));
    CHECK(OWFS_STATE_CLEAN == 0x0000u);
    CHECK(OWFS_STATE_DIRTY == 0x0001u);
    CHECK(OWFS_STATE_ERROR == 0x0002u);
    CHECK(OWFS_STATE_LOCKED == 0x0004u);
    CHECK(USFS_MAGIC == 0x55534653UL);   /* 'USFS' */

    /* On-disk geometry mirrors the canonical superblock/inode/catalog. */
    CHECK(sizeof(owfs_superblock_t) == 4096u);
    CHECK(sizeof(owfs_inode_t) == 256u);
    CHECK(sizeof(owfs_catalog_entry_t) == 256u);
    CHECK(offsetof(owfs_superblock_t, checksum) == 0x84u);
    CHECK(offsetof(owfs_catalog_entry_t, checksum) == 0xFCu);
}

int main(void) {
    test_constant_coexistence();
    test_mode_controller();
    test_boot_config_abi();
    test_owfs_mirrors();
    printf("compat_mbl: %d checks passed\n", g_checks);
    return 0;
}