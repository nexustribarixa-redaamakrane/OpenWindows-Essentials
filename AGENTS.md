# OpenWindows Project Memory & Guidelines

## Overview
**OpenWindows** is a custom, clean-slate, non-POSIX, NT-like operating system kernel and ecosystem designed as an open spiritual friend of Windows and a competitor to ReactOS, Linux, UNIX, and BSD.

## Core Architectural Principles

### 1. Freestanding C99 Standard
- **Toolchain constraints:** `-std=c99 -ffreestanding -nostdlib -Wall -Wextra -Werror`
- **Permitted headers:** `<stdint.h>`, `<stdbool.h>`, `<stddef.h>`, `<limits.h>`
- **No Standard Library:** All runtime utilities must be self-contained within the codebase.

### 2. Zero-Allocation Architecture
- **Zero dynamic heap allocations in core routines:** Core routines emphasize explicit memory structures and zero-allocation practices.
- **Memory Safety & Stability:** Pre-allocated static pools, caller-provided buffers, and fixed-layout structures are strictly enforced to maintain absolute system stability and predictable memory safety.

---

## Ecosystem Repositories (`Documents/`)

The OpenWindows ecosystem consists of interconnected repositories located in `C:\Users\KARIMABENDA\Documents\`:

| Repository | Purpose & Key Concepts |
|---|---|
| **`OpenWindows`** | Clean-slate non-POSIX NT-like kernel core. |
| **`OpenWindows-Essentials`** | Core driver infrastructure (`.owc`), freestanding dynamic libraries (`.owd`), `.sentinel` recovery directives & Damage Control engine. |
| **`Modular-Bootloader`** | Custom bootloader handling early boot memory maps, MBL diagnostics, and kernel handoff. |
| **`superunicode`** | Proprietary character encoding architecture (SUCS, SUTF-8/16/4/2, SUST transports, `.suf` font formats, SCP trap range `0x7FFFFFF0`–`0x7FFFFFFE`). |
| **`BANcode`** | Fault diagnostic and trap system (diagnostic codes, B+ range `0x0011A000`–`0x0011A7FF`, kernel security trap table). |
| **`OpenWindows-Storage`** | Native disk formats (**OWFS** and **USFS**), checksums (CRC32c/Fletcher), and `htl_device_t` storage interfaces. |
| **`vip`** | Volume Indexing Protocol (**UniVIP** and **FVIP** volume indexing, 512-byte sector addressing). |

---

## File Formats & Module Specs

| Extension | Magic | Role |
|---|---|---|
| `.owc` | `0x4F574331` | Bare-metal drivers and kernel modules (e.g. `fltrmgr`, `classpnp`). |
| `.owd` | `0x4F574431` | Freestanding dynamic link libraries (e.g. `htl`, `owrp`, `bootvid`). |
| `.sentinel`| `0x534E544C` | Crash recovery directives parsed by the Damage Control sentinel post-BANcode panic. |

---

## Interaction Rules for Coding Assistants
- Always adhere to freestanding C99 and zero-allocation constraints when generating kernel, driver, or DLL code.
- Cross-reference sibling ecosystems in `Documents/` (`superunicode`, `BANcode`, `vip`, `OpenWindows-Storage`, `Modular-Bootloader`) whenever integrating drivers, filesystems, diagnostics, boot protocols, or encodings.
