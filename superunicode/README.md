# superunicode (Super Universal Encoding System - SUES)

`superunicode` is a freestanding, 100% C99-compliant library providing the core character mapping and string serialization engine for the custom `OpenWindows` operating system.

---

## Architecture Overview

### 1. 31-Bit SUCS Address Space
- Code points (`sucs_char_t`) are 32-bit unsigned integers supporting a 31-bit address space (`0x00000000` to `0x7FFFFFFF`).
- Address breakdown: **128 Zones** -> **256 Districts** -> **256 Planes** -> **256 Block Offsets** (128 x 256 x 256 x 256 = 2^31).

### 2. Codepoint Classifications
- `0x00000000`–`0x0010FFFF`: **Unicode Bridge Zone** (1:1 standard UTF-8/Unicode parity).
- `0x00110000`–`0x0011FFFF`: **System Control Plane (SCP)** (Inline renderer instructions, style shifts, and layout markers).
- `0x00120000`–`0x7FFFFFFF`: **Native Extended SUCS Allocations** (Custom conlangs, RNUR multi-sets, neographies, technical symbols).

### 2b. UCS/UCB — Unicode Compatibility Space / Bridge

The bridge range `0x00000000`–`0x0010FFFF` is 1:1 with Unicode **by position**; the *data* published about that range tracks a concrete Unicode release. That release is queryable at compile time:

| Macro | Value | Meaning |
| :--- | :--- | :--- |
| `SUCS_UNICODE_VERSION_MAJOR` / `_MINOR` | `18` / `0` | Unicode release the UCS/UCB data is synchronized with |
| `SUCS_UCD_UNICODE_VERSION` | `1800` | Same, packed as `major * 100 + minor` |
| `SUCS_UCD_BLOCK_COUNT` | `353` | Unicode blocks in `sucs_compat.h` |
| `SUCS_UCD_NAME_COUNT` | `41232` | Named codepoints in `sucs_ucd_names.h` |

- `sucs_compat.h` — per-block `SUCS_UCD_BLOCK_*_MIN/_MAX` defines plus a sorted `sucs_ucd_blocks[]` table with `sucs_ucd_block_lookup()` (binary search; the table must stay strictly ascending and non-overlapping).
- `sucs_ucd_names.h/.c` — generated character-name database (`sucs_ucd_get_name()`, `sucs_ucd_name_length()`, `sucs_ucd_get_name_copy()`). The name pool is packed **without** NUL terminators, so `sucs_ucd_get_name()` returns a length-bounded slice; use `sucs_ucd_get_name_copy()` for a NUL-terminated string.

**Bumping to a new Unicode release** — run the name generator and hand-insert the new blocks in sorted order, then update the three expectations in `tests/test_sucs_ucd.c`:

```powershell
python tools/gen_ucd_names.py path\to\UnicodeData.txt 19.0
```

The generator derives the version banner from the path (or the second argument) and defaults to `SUCD_UNICODE_VERSION` in the script. Keep `SUCD_UNICODE_VERSION`, `SUCS_UNICODE_VERSION_*`, and the `test_sucs_ucd.c` expectations in sync — the test fails loudly when they drift.

### 3. BANcode Registry & Kernel Trap Damage Control Dispatch

The **BANcode Registry Plugin Range** (`0x0011A000`–`0x0011AEFF`) lives inside the System Control Plane (SCP). It is a Kernel Damage Control registry used when the OpenWindows kernel crashes or needs to report state:

| Range | Class | Width | Meaning |
| :--- | :--- | :--- | :--- |
| `0x0011A000`–`0x0011A7FF` | **B+ BANcode** | 2048 | Fatal kernel errors |
| `0x0011A800`–`0x0011ABFF` | **W+ WARNcode** | 1024 | Kernel warnings |
| `0x0011AC00`–`0x0011ADFF` | **C+ COMcode** | 512 | Success / Communications |
| `0x0011AE00`–`0x0011AEFF` | **S+ SOFTcode** | 256 | Soft errors |

#### Kernel Crash & Damage Control Workflow
1. **System Crash Event:** When the kernel crashes with a fatal B+ BANcode (`0x0011A000`–`0x0011A7FF`), it queries SuperUnicode to resolve the associated **Kernel Security Trap codepoint** (`0x7FFFFFF0`–`0x7FFFFFFE`).
2. **Trap Handler Dispatch:** Each of the 15 Kernel Security Trap slots (`0x7FFFFFF0`–`0x7FFFFFFE`) acts as a specialized Damage Control Handler governing its assigned cluster of 128 BANcodes.
3. **Data Dump & Recovery:** The resolved trap handler executes damage control, processes the crash state, and dumps diagnostic data tailored to its BANcode registry domain.

#### Dispatch API (`sucs_plane.h`)
- `sucs_classify_bancode(cp)` / `sucs_is_bancode_registry(cp)` / `sucs_is_bancode(cp)` / `sucs_is_warncode(cp)` / `sucs_is_comcode(cp)` / `sucs_is_softcode(cp)` — BANcode registry classification.
- `sucs_is_kernel_trap(cp)` — Kernel Security Trap range check (`0x7FFFFFF0`–`0x7FFFFFFE`).
- `sucs_bancode_to_trap(bancode_cp)` — resolves a fatal B+ BANcode to its Damage Control Trap codepoint (returns `SUCS_INVALID_CODEPOINT` when unmapped).
- `sucs_trap_to_bancode_range(trap_cp, *out_min, *out_max)` — returns the B+ BANcode cluster range governed by a trap handler.

### 4. SUTF Serialization Format (1 to 6 Bytes)
| Byte Count | Codepoint Range | Header Pattern | Payload Bits |
| :--- | :--- | :--- | :--- |
| 1 Byte | `0x00000000`–`0x0000007F` | `0xxxxxxx` | 7 bits |
| 2 Bytes | `0x00000080`–`0x000007FF` | `110xxxxx 10xxxxxx` | 11 bits |
| 3 Bytes | `0x00000800`–`0x0000FFFF` | `1110xxxx 10xxxxxx 10xxxxxx` | 16 bits |
| 4 Bytes | `0x00010000`–`0x0010FFFF` | `11110xxx 10xxxxxx 10xxxxxx 10xxxxxx` | 21 bits |
| 5 Bytes | `0x00110000`–`0x03FFFFFF` | `111110xx 10xxxxxx ...` | 26 bits |
| 6 Bytes | `0x40000000`–`0x7FFFFFFF` | `1111110x 10xxxxxx ...` | 31 bits |

---

## Directory Structure

```text
superunicode/
├── CMakeLists.txt
├── README.md
├── include/
│   └── superunicode/
│       ├── sucs_types.h
│       ├── sucs_plane.h
│       ├── sucs_compat.h
│       ├── sucs_trap.h
│       ├── sucs_ucd_names.h
│       ├── sutf.h
│       └── superunicode.h
├── src/
│   ├── sutf_encode.c
│   ├── sutf_decode.c
│   ├── sucs_string.c
│   ├── sucs_trap.c
│   ├── sucs_conv.c
│   └── sucs_ucd_names.c
├── tests/
│   ├── CMakeLists.txt
│   ├── test_sutf.c
│   ├── test_sucs_planes.c
│   ├── test_sucs_ucd.c
│   └── test_conv.c
├── tools/
│   ├── CMakeLists.txt
│   ├── gen_ucd_names.py
│   └── sucs_inspector.c
└── Public/
    └── 0.1.0/            # SUCD 0.1.0 data release
```

---

## Build & Test Instructions

### Building with CMake
```powershell
cmake -B superunicode/build -S superunicode
cmake --build superunicode/build
```

### Running Unit Tests
```powershell
ctest --test-dir superunicode/build --output-on-failure
```

### Using the Inspector CLI Tool
```powershell
./superunicode/build/tools/sucs_inspector 0x110000
./superunicode/build/tools/sucs_inspector 0x123456
```
