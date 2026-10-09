# Damage Control Sentinel Engine Specification

## Overview

The Damage Control Sentinel is the runtime recovery engine that parses and executes `.sentinel` directive files following a system crash. It is the last line of defense: after a BANcode panic halts normal execution, the sentinel loads recovery scripts, validates state integrity, and executes automated repair passes.

## Architecture

```
┌──────────────────────────────────────────────────────┐
│                  BANcode Panic                        │
│              (B+ 0x0011A000-0x0011A7FF)               │
└──────────────────────┬───────────────────────────────┘
                       │
                       ▼
┌──────────────────────────────────────────────────────┐
│           Damage Control Sentinel Engine              │
│                                                      │
│  1. Load .sentinel image from boot storage           │
│  2. Validate header + image checksum                 │
│  3. Validate state checksums                         │
│  4. Match trigger BANcode                            │
│  5. Execute directive sequence with retries          │
│  6. Log all results                                  │
└───┬──────┬──────┬──────┬──────┬──────┬──────────────┘
    │      │      │      │      │      │
    ▼      ▼      ▼      ▼      ▼      ▼
  OWFS   UniVIP  FVIP   HTL    OWRP   MBL
  shadow  trie   table  bus    ring   boot
  restore rebuild rebuild isolate portal data
```

## Recovery Flow

### Phase 1: Initialization

`dc_run_recovery()` receives a `.sentinel` image loaded in memory, along with a configuration and log buffer. It calls `dc_context_init()` which:

1. Validates magic bytes (`0x534E544C` / "SNTL")
2. Validates format version
3. Validates header checksum (CRC32c over `0x10`..`header_size`)
4. Validates full image checksum
5. Validates table bounds against image size
6. Validates BANcode range (`0x0011A000`–`0x0011A7FF`)
7. Populates internal pointers to directive, checksum, and trigger tables

### Phase 2: State Checksum Validation

`dc_validate_all_checksums()` iterates all checksum entries and verifies memory/storage region integrity. Each checksum uses one of:

- **CRC32c**: Primary algorithm, matches OWFS/USFS superblock integrity.
- **Fletcher-16/32**: Lightweight alternatives for constrained regions.
- **XOR-16**: Minimal integrity check for non-critical data.

Failures are logged but do not halt recovery unless `SENTINEL_COND_CHECKSUM_OK` conditions on subsequent directives fail.

### Phase 3: Trigger Matching

`dc_find_trigger()` scans the trigger table for an entry whose `bancode` matches the panic code. Each trigger maps to a starting directive index and a retry budget.

### Phase 4: Directive Execution

`dc_execute_all()` processes directives sequentially:

1. For each directive, check condition flags against current state.
2. If conditions pass, dispatch to the opcode handler.
3. If the directive fails and has `SENTINEL_COND_RETRY`, retry up to `max_retries` (from trigger) or `max_retry_per_trigger` (from config).
4. If any `SENTINEL_COND_CRITICAL` directive fails, abort recovery.
5. If `abort_on_first_fail` is set, abort on any failure.

### Phase 5: Result

Returns one of:
- `DC_RECOVERY_SUCCESS`: All directives passed.
- `DC_RECOVERY_PARTIAL`: Some directives failed but recovery completed.
- `DC_RECOVERY_FAILED`: Recovery aborted due to critical failure.
- `DC_RECOVERY_UNRECOVERABLE`: System cannot continue.
- `DC_RECOVERY_TIMEOUT`: Exceeded `max_recovery_time_ms`.

## Opcode Behaviors

### RESTORE_SHADOW (0x01)
Locates backup shadow file table via operand LBA offset, reads it from boot storage through HTL block I/O, writes it over the corrupt primary shadow table, and flushes the cache.

### ISOLATE_BUS (0x02)
Reads bus identifier from operand data, masks the bus from the PnP arbiter (classpnp.owc), disables all devices on the isolated bus, and logs the isolation event.

### LOG_STATE (0x03)
Writes a diagnostic snapshot to boot storage at the LBA/size specified in the operand. Uses VIP sector addressing (512 bytes/sector).

### REBUILD_FVIP (0x05)
Walks the UniVIP 44-bit hex trie, reconstructs the flat FVIP index table, validates against volume entries, and writes the rebuilt table to storage. Cross-references `vip/univip_fvip.h`.

### INVOKE_DRIVER (0x0D) / INVOKE_DLL (0x0E)
Resolves a driver/library by offset from the sentinel header, locates the handler function, and calls it with the directive operand as context. Used for driver-specific recovery logic.

## Caller-Provided Memory

The sentinel engine performs **zero heap allocations**. All memory is caller-provided:

- **`dc_context_t`**: Stack or static allocation, initialized by `dc_context_init()`.
- **`dc_config_t`**: Caller fills in configuration values.
- **`dc_recovery_log_t`**: Points to a caller-provided buffer that holds both log entries and a string table.

Log buffer layout:
```
┌─────────────────────────────────────────────┐
│ dc_log_entry_t × max_entries                │  ← log entries
├─────────────────────────────────────────────┤
│ String table (variable length)              │  ← message strings
└─────────────────────────────────────────────┘
```

## Integration Points

| System | Integration |
|--------|-------------|
| BANcode | Trigger matching on B+ codes; `RAISE_BANCODE` directive |
| VIP | Volume LBA addressing for storage I/O |
| OpenWindows-Storage | CRC32c/Fletcher checksums; OWFS shadow table restore |
| UniVIP/FVIP | FVIP rebuild from trie traversal |
| HTL | Block I/O for storage reads/writes |
| OWRP | Cross-ring service calls during recovery |
| MBL | Boot-phase detection; diagnostic data access |
| classpnp | Bus isolation via PnP arbiter interface |
| SuperUnicode | SCP trap range alignment (`0x7FFFFFF0`–`0x7FFFFFFE`) |
