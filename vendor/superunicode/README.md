# SuperUnicode (SUCS / ExtSUCS), SUTF Transformation Formats, & SUST Serialization Transports

> **OpenWindows System Architecture Core Library**  
> Bare-Metal C99 (`-std=c99 -nostdlib -ffreestanding`) implementation of SuperUnicode character encodings, SUTF transformation formats, and SUST serialization transports.

---

## Architecture Overview

### 1. Character Encodings vs. Transformation Formats vs. Serialization Transports

- **SuperUnicode (SUCS) and SuperUnicode Extended (ExtSUCS)**:  
  Strictly **CHARACTER ENCODINGS** defining numerical address spaces.
  - **Base SUCS**: 31-bit address space (`0x00000000` to `0x7FFFFFFF`). Reserved Kernel Security Trap Range: `0x7FFFFFF0`–`0x7FFFFFFE`. Sentinel: `0x7FFFFFFF` (`SUCS_INVALID_CODEPOINT`). Inside the SCP, the **BANcode Registry Plugin Range** (`0x0011A000`–`0x0011AEFF`) holds the kernel damage-control registry (B+ BANcode fatal errors `0x0011A000`–`0x0011A7FF`, W+ WARNcode `0x0011A800`–`0x0011ABFF`, C+ COMcode `0x0011AC00`–`0x0011ADFF`, S+ SOFTcode `0x0011AE00`–`0x0011AEFF`), dispatched to the 15 Kernel Security Trap handlers (`0x7FFFFFF0`–`0x7FFFFFFE`) via `sucs_bancode_to_trap()` / `sucs_trap_to_bancode_range()` for kernel crash damage control.
  - **ExtSUCS**: Unbounded address space (0 -> infinity, currently implemented via 64-bit `sucs_ex_char_t` container). Out-of-band error handling with zero in-band sentinels. Inherits Base SUCS trap range.

- **SUTF (Transformation Formats)**:  
  Strictly **TRANSFORMATION FORMATS** defining the endian-neutral mapping between SUCS codepoints and symbol sequences (byte words, hex nibbles, symbol frames). SUTF does NOT define physical byte ordering or framing — that is SUST's job.
  - **SUTF-8**: 1 to 6 Byte Variable Stream Transformation
  - **SUTF-16**: 1 to 2 16-Bit Word Transformation (1 word `0x0000`–`0x7FFF`, 2 words above; **no surrogates** — `0xD800`–`0xDFFF` are valid PUA codepoints encoded as 2 words, never paired)
  - **SUTF-4**: 4-Bit Hex Nibble Transformation for console & bus debugging
  - **SUTF-2**: 2-Bit Symbol Frame Transformation for IPC thread channels
  - **vSUTF**: Variable Multi-Byte Streaming Transformation for the full 64-bit ExtSUCS space

- **SUST (Serialization Transports)**:  
  Strictly the **SERIALIZATION TRANSPORT** layer defining physical byte-packing, bit-alignment, memory layouts, and stream framing.
  - **SUST-16**: Canonical BIG-ENDIAN byte serialization of the SUTF-16 word stream (2 or 4 bytes per codepoint, no BOM)
  - **SUST-32 / SUST-64**: 4B / 8B Fixed Vector Slot Transports (SIMD & AI Alignment)
  - **SUST-128 / 256 / 512 / N**: 16B / 32B / 64B / N-byte Aligned Vector Transports (SSE/AVX)
  - **e-SUST**: Hypervisor Page-Mapped Virtual IPC Transport (4096 codepoint pages, 6-byte IPC frame)

---

## Workspace Structure

```text
SuperUnicode/
├── CMakeLists.txt                      # Top-level unified CMake workspace configuration
├── README.md                           # Master project documentation
├── superunicode/                       # Base SUCS character encoding & inspector tools
│   ├── CMakeLists.txt
│   ├── include/superunicode/           # sucs_types.h, sucs_plane.h, sucs_compat.h, sucs_trap.h, sutf.h, superunicode.h
│   ├── src/                            # sutf_encode.c, sutf_decode.c, sucs_string.c, sucs_trap.c
│   ├── tests/                          # test_sutf.c, test_sucs_planes.c
│   └── tools/                          # sucs_inspector.c
├── sutf/                               # Bare-metal Base SUTF transformation formats & Kernel Mode Controller (libsutf.a)
│   ├── CMakeLists.txt
│   ├── include/                        # sutf.h, sucs_mode.h, sucs_types.h, sutf8/16/4/2.h
│   ├── src/                            # sucs_mode.c, sutf8.c, sutf16.c, sutf4.c, sutf2.c
│   └── tests/                          # test_sutf_all.c
├── superunicode_extended/              # ExtSUCS 64-bit encoding & vSUTF transformation (libsuperunicode_extended.a)
│   ├── CMakeLists.txt
│   ├── include/                        # extsucs_types.h, vsutf.h
│   ├── src/                            # vsutf.c
│   ├── plugin/                         # Plugin subsystem (libsuperunicode_plugin.a): checksum, staging, boot, partitions
│   └── tests/                          # test_extsutf_all.c
├── sust/                               # SUST serialization transports (libsust.a)
│   ├── CMakeLists.txt
│   ├── include/                        # sust.h, sust16.h, sustfixed.h, esust.h
│   ├── src/                            # sust16.c, sustfixed.c, esust.c
│   └── tests/                          # test_sust_all.c
├── unified/                            # Header coexistence test (links all 5 libraries in one TU)
│   ├── CMakeLists.txt
│   └── test_unified.c
└── website/                            # Static HTML documentation website
```

---

## Kernel Mode-Switching Subsystem (`sucs_mode.h`)

The OpenWindows Kernel mode controller allows switching between operating modes:
- **`SUCS_MODE_BASE` (0)**: 31-bit Base SUCS & Base SUTF (SUTF-8/16/4/2).
- **`SUCS_MODE_EXTENDED` (1)**: Unbounded ExtSUCS & vSUTF + SUST transports (SUST-32/64/128/256/512/N, e-SUST).

Mode alterations require a **mandatory system restart**:
1. `sucs_request_mode_switch(new_mode)` stages `pending_mode` and sets `reboot_required = true`.
2. `sucs_commit_mode_on_boot(&cfg)` commits the alteration during early kernel boot initialization.

---

## Building and Testing with CMake

### Prerequisites
- GCC / MinGW C99 compiler (`-std=c99 -nostdlib -ffreestanding`)
- CMake 3.10+

### Build Instructions
```bash
# Configure build tree
cmake -B build -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc

# Build static libraries (libsutf.a, libsuperunicode_extended.a, libsust.a, libsuperunicode_static.a)
cmake --build build

# Execute full test suite
ctest --test-dir build --output-on-failure
```

---

## License & Integration

Designed for core integration into the OpenWindows Operating System Kernel (`OpenWindows-Kernel`), bare-metal platforms, compilers, and system utilities.

Dual-licensed under either of:
- **MIT License** ([LICENSE](LICENSE))
- **Apache License, Version 2.0** ([LICENSE](LICENSE))

at your option.

