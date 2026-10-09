# SUST (SuperUnicode Serialization Transports)

> **OpenWindows System Architecture Core Library**  
> Bare-metal C99 (`-std=c99 -nostdlib -ffreestanding`) implementation of the SUST serialization transport layer (`libsust.a`).

---

## Architectural Role & Overview

In the OpenWindows system architecture, a strict distinction is maintained between **Character Encodings**, **Transformation Formats**, and **Serialization Transports**:

* **SUCS / ExtSUCS (Character Encodings):** Abstract codepoint numerical address spaces (31-bit Base, unbounded 64-bit Extended).
* **SUTF / extSUTF (Transformation Formats):** The endian-neutral mapping between codepoints and symbol sequences (defined in `sutf/` and `superunicode_extended/`).
* **SUST (Serialization Transports):** The physical byte-packing, bit-alignment, memory layouts, and stream framing rules for storing and transmitting codepoints across memory, SIMD vector registers, IPC channels, and low-level hardware buses. **This library.**

SUST is a strict SUPERSET layer over the transformation formats: you use SUTF to transform a codepoint into its symbol sequence (words, nibbles, frames), and SUST to decide how those symbols are physically placed on a byte medium.

---

## Transports

| Transport | Header | Unit | Byte Order | Description |
| :--- | :--- | :--- | :--- | :--- |
| **SUST-16** | `<sust16.h>` | 2 or 4 Bytes | BE (canonical) or LE, explicit | Byte serialization of the SUTF-16 word stream. No BOM (every word ≥ 0x8000 is a marker). API: `sust16_codepoint_bytes`, `sust16_encode_bytes`/`sust16_decode_bytes` (canonical BE), `sust16_encode_bytes_be`/`sust16_decode_bytes_be` (explicit BE), `sust16_encode_bytes_le`/`sust16_decode_bytes_le` (LE). |
| **SUST-32 / SUST-64** | `<sustfixed.h>` | 4B / 8B | Big-Endian, zero-padded | Fixed-width vector slots for 32-bit fast-path and 64-bit SIMD / AI tensor alignment. |
| **SUST-128 / 256 / 512** | `<sustfixed.h>` | 16B / 32B / 64B | Big-Endian, zero-padded | Aligned SIMD vector register slots (SSE / AVX-256 / AVX-512). |
| **SUST-N** | `<sustfixed.h>` | N Bytes (N ≥ 8) | Big-Endian, zero-padded | Arbitrary multi-word block container. |
| **e-SUST** | `<esust.h>` | 6-Byte IPC Frame | Big-Endian | Hypervisor page-mapped virtual coordinate translation (4096 codepoint pages) across IPC boundaries. |

---

## Directory Structure

```text
sust/
├── CMakeLists.txt              # Build configuration for libsust.a & unit tests
├── include/                    # Public C99 headers
│   ├── sust.h                  # Master aggregation header
│   ├── sust16.h                # SUST-16 byte serialization of SUTF-16 words
│   ├── sustfixed.h             # SUST-32/64/128/256/512/N fixed-width transports
│   └── esust.h                 # e-SUST page-mapped IPC transport
├── src/                        # Freestanding C99 implementation
│   ├── sust16.c                # SUST-16 encoder / decoder
│   ├── sustfixed.c             # Fixed-width vector encoders / decoders
│   └── esust.c                 # e-SUST page translation & IPC frame handlers
└── tests/
    └── test_sust_all.c         # Unit test suite for all serialization transports
```

The SUST library builds against `sucs_types.h` (Base SUCS, from `sutf/`) and `extsucs_types.h` (ExtSUCS, from `superunicode_extended/`); those modules' include directories are on the include path. SUST-16 transforms the word stream produced by the SUTF-16 transformation (`sutf16_encode_char`/`sutf16_decode_char`).

---

## Usage Examples

### SUST-16 Byte Serialization

```c
#include "sust.h"

sucs_char_t cp = 0x1234;         /* transform result of a SUTF-16 encode */
uint8_t frame[4];
size_t nb = sust16_encode_bytes(cp, frame, sizeof(frame));  /* 2 bytes BE: 12 34 */

sucs_char_t decoded;
size_t rb = sust16_decode_bytes(frame, nb, &decoded);
assert(rb == nb && decoded == cp);
```

### SUST-128 SIMD Slot

```c
#include "sust.h"

sucs_ex_char_t ex_cp = 0x123456789ABCULL;
uint8_t slot[SUST128_BYTES];     /* 16-byte SSE slot */
size_t written = sust128_encode(ex_cp, slot, sizeof(slot));

sucs_ex_char_t decoded;
size_t read = sust128_decode(slot, written, &decoded);
```

---

## Building and Testing

The SUST module references the sibling `sutf/` and `superunicode_extended/` include directories, so it must be built from the workspace root:

```bash
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
```

### Run Unit Tests

```bash
ctest --test-dir build --output-on-failure
```