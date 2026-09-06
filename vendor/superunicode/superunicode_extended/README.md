# SuperUnicode Extended (ExtSUCS & vSUTF)

> **OpenWindows System Architecture Core Library**  
> Bare-metal C99 (`-std=c99 -nostdlib -ffreestanding`) implementation of the unbounded 64-bit ExtSUCS character encoding and the vSUTF transformation format (`libsuperunicode_extended.a`).

---

## Architectural Role & Overview

`superunicode_extended` provides the high-performance extended character encoding for the OpenWindows system architecture:

* **ExtSUCS (Character Encoding):** Strictly an abstract, unbounded numerical address space (0 -> infinity). Implemented in C99 via the 64-bit `sucs_ex_char_t` container, supporting up to 2^64-1 codepoints. Inherits the System Control Plane (SCP) boundaries (`0x00110000`–`0x0011FFFF`) and, inside the SCP, the **BANcode Registry Plugin Range** (`0x0011A000`–`0x0011AEFF`) exactly as defined in Base SUCS.
* **vSUTF (Transformation Format):** Variable multi-byte transformation covering the full 64-bit ExtSUCS range, with a fast-path for Base SUCS.

> [!IMPORTANT]
> **Serialization transports live in SUST:** The fixed-width vector transports (SUST-32/64/128/256/512/N) and the hypervisor page-mapped IPC transport (e-SUST) were relocated to the SUST library. See `sust/` in the workspace root and `<sust.h>` — do not reference `extsutf_fixed.h` or `esutf.h`, they no longer exist.

> [!NOTE]
> **Out-of-Band Error Model:** In ExtSUCS, `0x7FFFFFFF` is a valid codepoint address (unlike 31-bit Base SUCS where `0x7FFFFFFF` is `SUCS_INVALID_CODEPOINT`). Because ExtSUCS is unbounded, it contains **zero in-band sentinels**. All functions return status flags (`bool` / `size_t`) and pass decoded codepoints via output pointers.

---

## Core Modules

### 1. ExtSUCS Character Encoding (`extsucs_types.h`)
* **Address Space:** 0 -> infinity (64-bit container `sucs_ex_char_t`).
* **Hardware Traps:** Inherits the Base SUCS Kernel Security Trap Range (`0x7FFFFFF0`–`0x7FFFFFFE`), which remains reserved across both modes.
* **System Control Plane & BANcode Registry:** Inherits the SCP (`0x00110000`–`0x0011FFFF`) and the BANcode Registry (`0x0011A000`–`0x0011AEFF`; B+ BANcode `0x0011A000`–`0x0011A7FF`, W+ WARNcode `0x0011A800`–`0x0011ABFF`, C+ COMcode `0x0011AC00`–`0x0011ADFF`, S+ SOFTcode `0x0011AE00`–`0x0011AEFF`), plus classification helpers (`extsucs_is_scp_plane`, `extsucs_is_bancode_registry`, `extsucs_classify_bancode`, ...).
* **Casting Utilities:**
  * `sucs_upcast(cp)`: Zero-cost widening conversion from 31-bit Base SUCS to 64-bit ExtSUCS.
  * `sucs_downcast(ex_cp, &out_cp)`: Safe narrowing conversion from 64-bit ExtSUCS to 31-bit Base SUCS. Fails out-of-band if `ex_cp > 0x7FFFFFFF` or equals `0x7FFFFFFF`.

---

### 2. Variable Streaming Transformation (`vsutf.h`)
`vSUTF` provides variable-length multi-byte transformation for the entire 64-bit ExtSUCS space with a fast-path for Base SUCS:

* **Base SUCS Codepoints (`0x0`–`0x7FFFFFFF`):** Transformed using standard SUTF-8 framing (1 to 6 bytes).
* **Extended Codepoints (`> 0x7FFFFFFF`):** Transformed using a 9-byte frame (`0xFE` prefix header + 8 bytes big-endian payload).
* **Reserved Prefix:** `0xFF` is reserved for future extensions beyond 64-bit address spaces.

---

## Directory Structure

```text
superunicode_extended/
├── CMakeLists.txt              # Build configuration for libsuperunicode_extended.a & tests
├── include/                    # Public C99 headers
│   ├── extsucs_types.h         # ExtSUCS 64-bit type, validation, upcast/downcast
│   └── vsutf.h                 # Variable multi-byte streaming transformation (vSUTF)
├── src/                        # Freestanding C99 implementation
│   ├── vsutf.c                 # vSUTF encoder / decoder
│   ├── extsucs_ucd_names.c     # Extended UCD name lookup
│   └── extsucs_conv.c          # UTF-8 <-> vSUTF / SUCS <-> ExtSUCS conversion
└── tests/
    └── test_extsutf_all.c      # ExtSUCS validators, SCP/BANcode, upcast/downcast, vSUTF tests
```

> **Fixed-width (SUST-32/64/128/256/512/N) and e-SUST IPC transports:** see the `sust/` sub-project (headers `sustfixed.h`, `esust.h`, aggregate `sust.h`).

---

## Code Examples

### ExtSUCS Upcasting & Safe Downcasting

```c
#include "extsucs_types.h"

// Zero-cost widening cast
sucs_char_t base_cp = 0x10000;
sucs_ex_char_t ex_cp = sucs_upcast(base_cp);

// Out-of-band safe narrowing cast
sucs_char_t out_base;
if (sucs_downcast(ex_cp, &out_base)) {
    // Successfully downcasted to Base SUCS
} else {
    // Codepoint exceeds 31-bit limit or falls in trap/sentinel range
}
```

### vSUTF Streaming Transformation

```c
#include "vsutf.h"

uint8_t stream_buf[VSUTF_MAX_BYTES]; // 9 bytes max
sucs_ex_char_t ex_cp = 0xFF88990011223344ULL;

// Transforms using 0xFE prefix header + 8 bytes payload
size_t bytes_written = vsutf_encode(ex_cp, stream_buf, sizeof(stream_buf));

sucs_ex_char_t decoded_cp;
size_t bytes_read = vsutf_decode(stream_buf, bytes_written, &decoded_cp);
```

> For SUST serialization transports (fixed-width SUST-32/64/128/256/512/N, e-SUST page-mapped IPC), see `sust/README.md`.

---

## Building and Testing

### Build `libsuperunicode_extended.a` Static Library
```bash
cmake -B superunicode_extended/build -S superunicode_extended -G "MinGW Makefiles"
cmake --build superunicode_extended/build
```

### Run Unit Tests
```bash
ctest --test-dir superunicode_extended/build --output-on-failure
```
