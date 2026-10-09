# OpenWindows Dynamic Link Library (.owd) Format Specification

## Overview

The `.owd` (OpenWindows Dynamic) file format defines the binary layout for freestanding user/kernel dynamic link libraries. These libraries provide portable subroutines that hide board-specific hardware logic (HTL), map user requests across privilege levels (OWRP), and draw directly to framebuffers during early boot (bootvid).

Every `.owd` image begins with a fixed 192-byte header, followed by symbol tables, relocation entries, dependency references, and code/data sections.

## File Layout

```
Offset  Size    Description
------  ------  -----------
0x0000  192     Fixed header (owd_header_t)
0x00C0  var     Dependency table (owd_dependency_entry_t × dependency_count)
var     var     Symbol table (owd_symbol_entry_t × symbol_count)
var     var     Relocation table (owd_reloc_entry_t × reloc_count)
var     var     String table
var     var     Code section
var     var     Data section
```

## Magic & Versioning

| Field          | Value               | Notes                              |
|----------------|----------------------|------------------------------------|
| Magic          | `0x4F574431`         | ASCII "OWD1" little-endian        |
| Header Size    | `192`                | Fixed, always this value           |
| Format Version | `0x0001`             | Major = high byte, Minor = low byte|

## Library Types

| Value | Type     | Description                                      |
|-------|----------|--------------------------------------------------|
| 0x00  | USER     | Userspace library                                |
| 0x01  | KERNEL   | Kernel-mode library                              |
| 0x02  | HYBRID   | Dual-mode (user + kernel entry points)           |
| 0x03  | BOOT     | Early boot library (loaded before filesystem)    |

## Base Address Constraints

Libraries are loaded within fixed memory ranges depending on their type:

| Type   | Min Address  | Max Address  | Notes                              |
|--------|-------------|-------------|-------------------------------------|
| KERNEL | `0x00100000` | `0x01000000` | Above bootloader, below 16 MiB     |
| USER   | `0x10000000` | `0x80000000` | 256 MiB to 2 GiB                   |
| BOOT   | `0x00051000` | `0x00100000` | After MBL_BOOTCONFIG, below kernel |

These ranges are derived from the Modular-Bootloader memory map (`mbl.h`):
- `MBL_BOOTCONFIG = 0x510`
- `MBL_KERNEL_ADDR = 0x200000`
- `MBL_KERNEL_MAX = 0x1000000`

## Header Fields

Key header fields:

- **base_address**: Preferred load address. Must fall within the range for the library's type. Address `0` means position-independent (any valid address).
- **entry_point / entry_point_user**: Kernel-mode and user-mode entry points respectively. `entry_point_user` is `0` for pure kernel libraries.
- **alignment_log2**: Required alignment as a power of 2 (e.g., `12` = 4 KiB page alignment).
- **init_flags**: Bitmask of `owc_init_requirement_t` (shared with .owc format).

## Symbol Table

Each symbol entry is 32 bytes:

- **address**: Resolved at load time by the dynamic linker.
- **visibility**: `OWD_SYM_LOCAL` (not exported), `OWD_SYM_GLOBAL`, `OWD_SYM_WEAK` (overridable), `OWD_SYM_KERNEL` (kernel-only).
- **size**: Symbol size in bytes; `0` means unknown.
- **checksum**: CRC32c of the symbol target for integrity verification.

## Relocation Table

Each relocation entry is 16 bytes:

- **offset**: Byte offset within the image to patch.
- **symbol_index**: Index into the symbol table.
- **reloc_type**: One of:
  - `OWD_RELOC_ABS32/ABS64`: Absolute address fixup.
  - `OWD_RELOC_REL32/REL64`: PC-relative fixup.
  - `OWD_RELOC_BASE_REF`: Reference to the library's base address.
  - `OWD_RELOC_HTLCALL`: HTL thunk relocation (routes through `htl.owd`).
  - `OWD_RELOC_OWRPCALL`: OWRP gate call relocation (routes through `owrp.owd`).

## Dependencies

Each dependency entry is 16 bytes specifying a required library by name (via string table), with min/max version constraints.

## Cross-Repository Alignment

- **OpenWindows-Storage** (`Documents/OpenWindows-Storage`): `io_version` aligns with OWFS/USFS subsystem versions. The HTL library (`htl.owd`) wraps `htl_device_t` and `htl_status_t` from `ow_htl.h`.
- **Modular-Bootloader** (`Documents/Modular-Bootloader`): BOOT-type libraries are loaded in the range `0x51000`–`0x100000` per `mbl.h` memory map. MBL font data (`efi_font_data`) is available to `bootvid.owd`.
- **VIP** (`Documents/vip`): `vip_version` aligns with UniVIP/FVIP protocol versions.
- **SuperUnicode** (`Documents/superunicode`): SCP trap range shared with BANcode trap table for sentinel integration.

## Validation

Use `owd_header_valid()` (inline in the header) to validate magic, header size, symbol/reloc/dependency counts, and base address constraints before parsing tables.
