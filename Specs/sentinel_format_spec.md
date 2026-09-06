# Sentinel Recovery Directive (.sentinel) Format Specification

## Overview

The `.sentinel` file format defines the binary layout for crash-state recovery scripts executed by the Damage Control Sentinel engine after a system panic triggered by a BANcode fault. Each file describes a sequence of repair operations (directives), state checksums for corruption validation, and fallback triggers.

Sentinel files are generated at build time for each driver/library that opts into sentinel recovery, and are loaded from boot storage during the recovery phase.

## File Layout

```
Offset  Size    Description
------  ------  -----------
0x0000  128     Fixed header (sentinel_header_t)
0x0080  var     Directive table (sentinel_directive_header_t × directive_count)
var     var     Checksum table (sentinel_checksum_entry_t × checksum_count)
var     var     Trigger table (sentinel_trigger_entry_t × trigger_count)
var     var     String table
var     var     Directive operand data blocks
```

## Magic & Versioning

| Field          | Value               | Notes                              |
|----------------|----------------------|------------------------------------|
| Magic          | `0x534E544C`         | ASCII "SNTL" little-endian        |
| Header Size    | `128`                | Fixed, always this value           |
| Format Version | `0x0001`             | Major = high byte, Minor = low byte|

## Header Fields

The header (`sentinel_header_t`) occupies exactly 128 bytes at offset 0x0:

- **trigger_bancode**: The B+ BANcode that activates this recovery script. Must be in range `0x0011A000`–`0x0011A7FF`.
- **trigger_trap_index**: Index into the system trap table for recovery dispatch.
- **directive_count / checksum_count / trigger_count**: Counts of respective tables.
- **target_volume_lba / target_volume_size**: The affected volume in VIP sector units (512 bytes/sector).
- **associated_owc_offset / associated_owd_offset**: Offsets to embedded driver/library references if the recovery requires specific modules.
- **max_recovery_time_ms**: Hard timeout for the entire recovery sequence.
- **flags**: Bitmask of `SENTINEL_FLAG_*` controlling recovery behavior.

## Directive Opcodes

Each directive is preceded by a 16-byte header (`sentinel_directive_header_t`) specifying the opcode, operand size, condition flags, and checksum index.

| Opcode | Name                | Description                                |
|--------|---------------------|--------------------------------------------|
| 0x00   | NOOP                | No operation (padding/placeholder)         |
| 0x01   | RESTORE_SHADOW      | Restore shadow file table from backup      |
| 0x02   | ISOLATE_BUS         | Isolate a faulted hardware bus             |
| 0x03   | LOG_STATE           | Write diagnostic state to boot storage     |
| 0x04   | RESET_DEVICE        | Hardware reset of a specific device        |
| 0x05   | REBUILD_FVIP        | Rebuild FVIP index from UniVIP trie        |
| 0x06   | FLUSH_CACHE         | Flush and invalidate cache range           |
| 0x07   | CHECKPOINT          | Write recovery checkpoint for rollback     |
| 0x08   | ROLLBACK            | Restore to a previous checkpoint           |
| 0x09   | SHUTDOWN_FAIL       | Controlled shutdown on unrecoverable fault |
| 0x0A   | RELOCATE            | Relocate data from bad sectors to spares   |
| 0x0B   | RAISE_BANCODE       | Raise a BANcode for secondary handler      |
| 0x0C   | WAIT_EVENT          | Block until hardware event fires           |
| 0x0D   | INVOKE_DRIVER       | Call into a specific .owc driver handler   |
| 0x0E   | INVOKE_DLL          | Call into a specific .owd library handler  |

## Condition Flags

Directives carry condition flags that determine when they execute:

| Flag                   | Value    | Description                            |
|------------------------|----------|----------------------------------------|
| COND_NONE              | 0x0000   | Unconditional                         |
| COND_CHECKSUM_OK       | 0x0001   | Only if referenced checksum passes    |
| COND_CHECKSUM_BAD      | 0x0002   | Only if referenced checksum fails     |
| COND_RETRY             | 0x0004   | Retriable (per trigger retry limit)   |
| COND_CRITICAL          | 0x0008   | Failure aborts entire recovery        |
| COND_BOOT_ONLY         | 0x0010   | Only during boot phase                |
| COND_HOT_PLUG          | 0x0020   | Can execute while system is live      |

## State Checksums

Each checksum entry (16 bytes) describes a memory or storage region to validate:

- **address**: Physical or sector address to read.
- **size**: Number of bytes to cover.
- **expected_value**: 32-bit expected checksum split across lo/hi fields.
- **checksum_type**: Algorithm to use (CRC32c, Fletcher-16, Fletcher-32, XOR-16).

Checksum algorithms match those used by OpenWindows-Storage for superblock integrity (CRC32c and Fletcher-64).

## Fallback Triggers

Each trigger entry (16 bytes) maps a BANcode to a starting directive index:

- **bancode**: The B+ BANcode that fires this trigger.
- **directive_index**: Index into the directive array to begin execution.
- **max_retries**: Maximum retry attempts before abort.
- **flags**: `ABORT_ON_MAX`, `LOG_ONLY`, `REBOOT`.

## BANcode Integration

The sentinel format is tightly coupled with the BANcode fault system:

- **Trigger range**: B+ BANcodes `0x0011A000`–`0x0011A7FF` (from `bancode_all.h`).
- **Trap range**: `0x7FFFFFF0`–`0x7FFFFFFE`, 15 slots (from `bancode_all.h`).
- **SuperUnicode trap range** (`sucs_types.h`): `0x7FFFFFF0`–`0x7FFFFFFE`, shared with BANcode.
- **MBL BANcode placements** (`mbl_bancode.h`): `MBL_BAN_*` codes at `0x0011A2E0+` within the B+ range.

When a BANcode panic fires, the Damage Control Sentinel scans all available `.sentinel` files for a matching `trigger_bancode`, then executes the directive sequence.

## Cross-Repository Alignment

- **BANcode** (`Documents/BANcode`): B+ range `0x0011A000`–`0x0011A7FF`, W+ `0x0011A800`–`0x0011ABFF`, C+ `0x0011AC00`–`0x0011ADFF`, S+ `0x0011AE00`–`0x0011AEFF`. Sentinel triggers only on B+ codes.
- **VIP** (`Documents/vip`): `VIP_SECTOR_SIZE = 512`, `VIP_MBL_RESERVED_LBA_COUNT = 128`, `VIP_OWFS_PARTITION_LBA = 131200`. Volume addresses in sentinel headers use these LBA conventions.
- **OpenWindows-Storage** (`Documents/OpenWindows-Storage`): OWFS superblock uses CRC32c + Fletcher-64 for integrity. Sentinel checksums use the same algorithms. OWFS block size `0x1000`, USFS block size `0x1000`.
- **Modular-Bootloader** (`Documents/Modular-Bootloader`): MBL boot config magic `0x324C424D`. Sentinel recovery may reference MBL diagnostic data (`mbl_diag_t`).
- **SuperUnicode** (`Documents/superunicode`): SCP trap range `0x7FFFFFF0`–`0x7FFFFFFE` aligns with sentinel trap indices.
