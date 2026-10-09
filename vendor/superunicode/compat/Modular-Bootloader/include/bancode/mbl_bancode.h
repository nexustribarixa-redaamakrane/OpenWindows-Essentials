/* mbl_bancode.h - MBL boot-path diagnostics (BANcode registry placements)
 *
 * Companion to <bancode/bancode_all.h>. Lives in its OWN header (rather
 * than inside the vendored bancode_all.h mirror) so the upstream BANcode
 * master header can own the shared BANCODE_ALL_H guard without hiding
 * these definitions - whichever bancode_all.h wins inclusion order,
 * the MBL_* placements below always apply.
 *
 * All codes are PROVISIONAL placements inside the canonical registry
 * blocks:
 *   B+ BANcode   0x0011A000-0x0011A7FF  fatal corruption -> kernel halt
 *   W+ WARNcode  0x0011A800-0x0011ABFF  non-fatal telemetry
 *   C+ COMcode   0x0011AC00-0x0011ADFF  success / completion reports
 *   S+ SOFTcode  0x0011AE00-0x0011AEFF  recoverable soft faults
 * No slots are claimed in the upstream registries yet; names stay
 * stable when official slots are assigned. Neighborhoods avoid the
 * concrete ranges already staked out by vip (VIP_* at 0x0011AD00+,
 * 0x0011AEE0+, 0x0011A3E0+).
 */
#ifndef MBL_BANCODE_PROVISIONAL_H
#define MBL_BANCODE_PROVISIONAL_H

#include <stdint.h>

/* -- Success codes (C+ COMcode block: boot & storage completion reports,
 *    provisional placement at 0x0011AC20+, clear of VIP's 0x0011AD00+) --- */
#define MBL_COM_VOLUME_PROBE_OK     0x0011AC20U  /* OWFS volume probed & verified */
#define MBL_COM_CATALOG_ENUM_OK     0x0011AC21U  /* root catalog enumerated */
#define MBL_COM_KERNEL_LOAD_OK      0x0011AC22U  /* kernel payload streamed to RAM */
#define MBL_COM_BOOT_HANDOFF_OK     0x0011AC23U  /* boot config published */

/* -- Non-fatal telemetry (W+ WARNcode block: volume condition reports,
 *    provisional placement at 0x0011AA20+) -------------------------------- */
#define MBL_WARN_VOLUME_DIRTY       0x0011AA20U  /* state_flags has DIRTY bit */
#define MBL_WARN_VOLUME_LOCKED      0x0011AA21U  /* volume locked / consistency scan */
#define MBL_WARN_STATE_ERROR        0x0011AA22U  /* superblock reports ERROR state */
#define MBL_WARN_VERSION_MISMATCH   0x0011AA23U  /* minor version differs from spec */

/* -- Recoverable soft faults (S+ SOFTcode block: boot can proceed or be
 *    retried, provisional placement at 0x0011AEA0+) ----------------------- */
#define MBL_SOFT_NO_VOLUME          0x0011AEA0U  /* no OWFS volume on boot drive */
#define MBL_SOFT_IS_USFS            0x0011AEA1U  /* USFS removable-media volume found instead */
#define MBL_SOFT_NOT_A_FILE         0x0011AEA2U  /* selected entry is not a regular file */
#define MBL_SOFT_KERNEL_TOO_SMALL   0x0011AEA3U  /* kernel image below minimum size */
#define MBL_SOFT_KERNEL_TOO_LARGE   0x0011AEA4U  /* kernel image exceeds load cap */
#define MBL_SOFT_ENCRYPTED_VOLUME   0x0011AEA5U  /* ChaCha20 volume - no key material */
#define MBL_SOFT_BAD_BLOCK_MAP      0x0011AEA6U  /* missing block map entry / hole */

/* -- Fatal corruption (B+ BANcode block: storage-integrity neighborhood,
 *    provisional placement at 0x0011A2E0+, clear of VIP's 0x0011A3E0+) ---- */
#define MBL_BAN_SB_MAGIC            0x0011A2E0U  /* superblock magic invalid */
#define MBL_BAN_SB_CHECKSUM         0x0011A2E1U  /* superblock CRC32c mismatch */
#define MBL_BAN_INODE_CHECKSUM      0x0011A2E2U  /* inode CRC32c mismatch */
#define MBL_BAN_CATALOG_CHECKSUM    0x0011A2E3U  /* catalog entry CRC32c mismatch */
#define MBL_BAN_IO_ERROR            0x0011A2E4U  /* Block I/O sector read failure */
#define MBL_BAN_LOAD_FAILED         0x0011A2E5U  /* kernel stream to RAM failed */

#endif /* MBL_BANCODE_PROVISIONAL_H */
