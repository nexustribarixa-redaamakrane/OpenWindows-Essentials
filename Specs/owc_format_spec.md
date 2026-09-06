# OpenWindows Component (.owc) Format Specification

## Overview

The `.owc` (OpenWindows Component) file format defines the binary layout for bare-metal hardware drivers, kernel modules, and protocol arbiters within the OpenWindows-Essentials ecosystem. Every `.owc` image is a self-contained, position-independent binary blob with a fixed 256-byte header followed by loadable sections, export/import tables, and a string table.

## File Layout

```
Offset  Size    Description
------  ------  -----------
0x0000  256     Fixed header (owc_header_t)
0x0100  var     Section table (owc_section_entry_t × section_count)
0x0100+N var   Export table (owc_export_entry_t × export_count)
var     var     Import table
var     var     String table
var     var     Section data (code, data, reloc, debug, sentinel)
```

## Magic & Versioning

| Field          | Value               | Notes                              |
|----------------|----------------------|------------------------------------|
| Magic          | `0x4F574331`         | ASCII "OWC1" little-endian        |
| Header Size    | `256`                | Fixed, always this value           |
| Format Version | `0x0001`             | Major = high byte, Minor = low byte|

## Header Fields

The header (`owc_header_t`) occupies exactly 256 bytes at offset 0x0. Key fields:

- **entry_point / init_entry / unload_entry**: Byte offsets from image start to driver entry, initialization, and unload functions.
- **section_count / export_count / import_count**: Counts of respective tables.
- **init_flags**: Bitmask of `owc_init_requirement_t` indicating which subsystems this driver requires (HTL, OWRP, VIP, BANcode, MBL, font).
- **driver_flags**: Bitmask of `OWC_FLAG_*` attributes (PnP-aware, boot driver, filter driver, DMA, privileged, sentinel-aware).
- **sentinel_bancode / sentinel_trap_index / sentinel_entry**: Integration with the Damage Control Sentinel recovery system.
- **io_version / vip_version / bancode_version**: Minimum required versions of dependent subsystems.

## Section Types

| ID   | Name        | Description                                |
|------|-------------|--------------------------------------------|
| 0x01 | CODE        | Executable code section                    |
| 0x02 | DATA        | Initialized read/write data                |
| 0x03 | BSS         | Zero-initialized data                      |
| 0x04 | RELOC       | Relocation table                           |
| 0x05 | EXPORT      | Export symbol table                        |
| 0x06 | IMPORT      | Import dependency table                    |
| 0x07 | DEBUG       | Debug/diagnostic metadata                  |
| 0x08 | SENTINEL    | Embedded sentinel recovery directives      |

## Export Table

Each export entry is 32 bytes and contains:

- **address**: Absolute virtual address of the exported symbol.
- **name_offset**: Offset into the string table.
- **ordinal**: Stable ordinal for ABI compatibility.
- **checksum**: CRC32c of the exported code/data.

Ordinals flagged with `OWC_EXPORT_ORDINAL_STABLE` must never change across versions.

## BANcode Sentinel Integration

The `.owc` header reserves fields for sentinel recovery:

- **sentinel_bancode**: The B+ BANcode (`0x0011A000`–`0x0011A7FF`) that triggers this driver's recovery handler.
- **sentinel_trap_index**: Index into the trap table (`0x7FFFFFF0`–`0x7FFFFFFE`).
- **sentinel_entry**: Offset to the recovery handler function within the driver.

If `OWC_FLAG_SENTINEL_AWARE` is set in `driver_flags`, the system will invoke the driver's sentinel entry point when the corresponding BANcode fires.

## Cross-Repository Alignment

- **BANcode** (`Documents/BANcode`): B+ range `0x0011A000`–`0x0011A7FF`, trap range `0x7FFFFFF0`–`0x7FFFFFFE`, 15 trap slots.
- **Modular-Bootloader** (`Documents/Modular-Bootloader`): `OWC_FLAG_BOOT_DRIVER` indicates early-boot loading; `init_flags` references MBL handoff data.
- **OpenWindows-Storage** (`Documents/OpenWindows-Storage`): `io_version` aligns with OWFS/USFS subsystem versions; OWFS block size `0x1000`, magic `0x4F574653`.
- **VIP** (`Documents/vip`): `vip_version` aligns with UniVIP/FVIP protocol versions; VIP sector size 512 bytes.
- **SuperUnicode** (`Documents/superunicode`): SCP trap range `0x7FFFFFF0`–`0x7FFFFFFE` shared with BANcode trap table.

## Validation

Use `owc_header_valid()` (inline in the header) to validate magic, header size, section/export counts, sentinel trap index, and BANcode range before parsing tables.
