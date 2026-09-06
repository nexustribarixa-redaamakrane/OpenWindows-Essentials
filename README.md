# OpenWindows-Essentials

Core driver infrastructure, dynamic libraries, file format specifications, and recovery engine for the OpenWindows microkernel ecosystem.

## Repository Structure

```
OpenWindows-Essentials/
├── Drivers/
│   ├── fltrmgr/          # Filter Manager (.owc) - I/O request interception
│   └── classpnp/         # PnP Arbiter (.owc) - device enumeration & bus negotiation
├── DLL/
│   ├── htl/              # Hardware Translation Layer (.owd) - portable HW interface
│   ├── owrp/             # OpenWindows Ring Portal (.owd) - cross-ring service calls
│   └── bootvid/          # Boot Video (.owd) - early boot framebuffer drawing
├── Software/             # (empty - executables added when specified)
├── Extensions/
│   ├── owc_format.h      # .owc binary header format (magic: 0x4F574331)
│   ├── owd_format.h      # .owd binary header format (magic: 0x4F574431)
│   ├── sentinel_format.h # .sentinel directive format (magic: 0x534E544C)
│   └── damage_control/
│       ├── damage_control.h   # Sentinel recovery engine API
│       └── damage_control.c   # Sentinel recovery engine implementation
├── Specs/
│   ├── owc_format_spec.md
│   ├── owd_format_spec.md
│   ├── sentinel_format_spec.md
│   └── damage_control_spec.md
├── CMakeLists.txt
└── README.md
```

## Build

Requires CMake 3.20+ and a freestanding C99 toolchain.

```bash
cmake -B build -DCMAKE_C_COMPILER=gcc
cmake --build build
```

All C code compiles with: `-std=c99 -ffreestanding -nostdlib -Wall -Wextra -Werror`

Driver and DLL targets are not built by default. Provide source files via CMake cache variables:

```bash
cmake -B build -DFLTRMGR_SOURCES=path/to/fltrmgr.c
cmake -B build -DHTL_SOURCES=path/to/htl.c
```

## File Formats

| Extension   | Magic       | Purpose                                    |
|-------------|-------------|--------------------------------------------|
| `.owc`      | `0x4F574331`| Bare-metal drivers and kernel modules      |
| `.owd`      | `0x4F574431`| Freestanding dynamic link libraries        |
| `.sentinel` | `0x534E544C`| Crash recovery directives for the sentinel |

See `Specs/` for full binary format documentation.

## Design Constraints

- **C99 freestanding**: `-std=c99 -ffreestanding -nostdlib`
- **Permitted headers**: `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<limits.h>`
- **Zero heap allocation**: All memory is pre-allocated static pools or caller-provided buffers
- **No standard library**: All functions are self-contained within the codebase

## Cross-Repository Alignment

This repository maintains compatibility with:

- **[superunicode](https://github.com/nexustribarixa-redaamakrane/superunicode)** - PUA character encoding, SCP trap range `0x7FFFFFF0`–`0x7FFFFFFE`
- **[OpenWindows-Storage](https://github.com/nexustribarixa-redaamakrane/OpenWindows-Storage)** - OWFS/USFS filesystem types, CRC32c/Fletcher checksums, `htl_device_t`
- **[Modular-Bootloader](https://github.com/nexustribarixa-redaamakrane/Modular-Bootloader)** - Boot memory map, MBL diagnostics, early boot handoff
- **[vip](https://github.com/nexustribarixa-redaamakrane/vip)** - UniVIP/FVIP volume indexing, VIP sector addressing (512 bytes)
- **[BANcode](https://github.com/nexustribarixa-redaamakrane/BANcode)** - Fault diagnostic codes, B+ range `0x0011A000`–`0x0011A7FF`, trap table

## Damage Control Sentinel

The sentinel engine (`Extensions/damage_control/`) reads `.sentinel` recovery scripts after a BANcode panic and executes automated repair passes: restoring shadow file tables, isolating faulted buses, rebuilding FVIP indexes, and logging diagnostic state. See `Specs/damage_control_spec.md` for architecture details.
