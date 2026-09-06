# TASK: Scaffold and Implement the Core `superunicode` Engine (SUCS / SUTF / SUES)

You are an expert Systems Programmer and Kernel Architect. You are tasked with creating a standalone, freestanding, 100% C99-compliant C library named `superunicode` (also known as the Super Universal Encoding System - SUES). 

This library will serve as the core string and character mapping engine for a custom, non-POSIX operating system (`OpenWindows`). It must be strictly FREESTANDING (`-ffreestanding`) with ZERO dependencies on the standard C library (`stdio.h`, `stdlib.h`, `string.h`), relying ONLY on compiler-provided headers (`stdint.h`, `stdbool.h`, `stddef.h`).

---

## 1. Architectural Rules & Requirements

1. **SUCS Code Point Space:** 
   - Code points (`sucs_char_t`) are 32-bit unsigned integers supporting a 31-bit valid address space (`0x00000000` to `0x7FFFFFFF`).
   - Addresses are mapped as: **128 Zones** -> **256 Districts** -> **256 Planes** -> **256 Block Offsets** (128 x 256 x 256 x 256 = 2^31).
   - **Unicode Compatibility Range:** `0x00000000` to `0x0010FFFF` (District 0 and first part of District 1) are treated as 1:1 Unicode compatible.
   - **Native SUCS Space:** Code points `> 0x0010FFFF` are native extended planes.
   - **Plane Types:** 
     - Fixed-Width (e.g., Planes 0 & 1 for system UI/kernel, rigid 256-block layouts).
     - Variable-Width (Planes 2+ for scripts, modern/historical languages, and custom symbols).

2. **SUTF Serialization Format (1 to 6 Bytes):**
   - Encode 31-bit code points into variable-length byte streams following UTF-8-like multi-byte continuation patterns extended up to 6 bytes:
     - 1 byte:  `0xxxxxxx` (7 bits, 0x00 - 0x7F)
     - 2 bytes: `110xxxxx 10xxxxxx` (11 bits)
     - 3 bytes: `1110xxxx 10xxxxxx 10xxxxxx` (16 bits)
     - 4 bytes: `11110xxx 10xxxxxx 10xxxxxx 10xxxxxx` (21 bits)
     - 5 bytes: `111110xx 10xxxxxx ...` (26 bits)
     - 6 bytes: `1111110x 10xxxxxx ...` (31 bits)

3. **Kernel-Safe Descriptor (`SUCS_STRING`):**
   - Strings are NOT guaranteed to be null-terminated. They must use explicit length-bounded descriptors:
     ```c
     typedef struct {
         uint32_t length_bytes;
         uint32_t capacity_bytes;
         char*    buffer;
     } SUCS_STRING;
     ```

---

## 2. Directory Structure to Generate

Create the following files in the project workspace:

```text
superunicode/
├── CMakeLists.txt
├── README.md
├── include/
│   └── superunicode/
│       ├── sucs_types.h
│       ├── sucs_plane.h
│       ├── sucs_compat.h
│       ├── sutf.h
│       └── superunicode.h
├── src/
│   ├── sutf_encode.c
│   ├── sutf_decode.c
│   └── sucs_string.c
├── tests/
│   ├── CMakeLists.txt
│   ├── test_sutf.c
│   └── test_sucs_planes.c
└── tools/
    └── sucs_inspector.c
```
---

## 3. Detailed Implementation Specs

### A. `include/superunicode/sucs_types.h`
- Define `typedef uint32_t sucs_char_t;`
- Define `SUCS_STRING` struct.
- Define bit-mask constants for 31-bit boundaries (`SUCS_MAX_CODEPOINT` -> `0x7FFFFFFF`).
- Define error status enums (`SUES_SUCCESS`, `SUES_ERR_INVALID_BYTE`, `SUES_ERR_BUFFER_TOO_SMALL`, `SUES_ERR_OUT_OF_BOUNDS`).

### B. `include/superunicode/sucs_plane.h`
- Define macros / inline functions for coordinate extraction:
    - `SUCS_GET_ZONE(cp)` -> Bits 24..30
    - `SUCS_GET_DISTRICT(cp)` -> Bits 16..23
    - `SUCS_GET_PLANE(cp)` -> Bits 8..15
    - `SUCS_GET_OFFSET(cp)` -> Bits 0..7
- Provide fast inline checks for `sucs_is_fixed_plane(cp)` (returns true for Planes 0 and 1).

### C. `include/superunicode/sucs_compat.h`
- Define `SUCS_UNICODE_MAX_COMPAT 0x0010FFFF`.
- Macro/inline function `sucs_is_unicode_compat(cp)` -> `(cp <= SUCS_UNICODE_MAX_COMPAT)`.
- Macro/inline function `sucs_is_native_extended(cp)` -> `(cp > SUCS_UNICODE_MAX_COMPAT)`.

### D. `include/superunicode/sutf.h` & Implementation in `src/`
- Implement `int sutf_encode_char(sucs_char_t cp, char* out_buf, size_t buf_size, size_t* out_bytes_written);`
- Implement `int sutf_decode_char(const char* in_buf, size_t buf_size, sucs_char_t* out_cp, size_t* out_bytes_read);`
- Ensure zero dynamic allocation (`malloc` is strictly forbidden in `src/`). Memory must be passed in by the caller.

### E. `src/sucs_string.c`
- Implement freestanding string operations operating on `SUCS_STRING*`:
    - `sucs_strlen()` (counts code points, not just bytes).
    - `sucs_strcpy()` (safe bounded copy).
    - `sucs_streq()` (string equality comparison).

### F. `tests/` & `tools/`
- Write unit tests in standard host C using `assert.h` (tests can use standard libraries since they run on the host development system).
- Implement `tools/sucs_inspector.c`: A command-line program that takes a hexadecimal code point (e.g., `0x123456`) and prints out:
    - Zone ID
    - District ID
    - Plane ID
    - SUTF Byte Hex Sequence (1 to 6 bytes)
    - Compatibility status (Unicode vs Native Extended)

## 4. Execution Step

- Generate all header files under `include/superunicode/`.
- Implement code files under `src/`.
- Create `CMakeLists.txt` configured to build a static freestanding library target `superunicode_static` and executable targets for `tests/` and `tools/`.
- Run the build and execute `tests/` to verify all bitmasking and 1-6 byte SUTF transformations pass without memory issues.

## 5. Strict Formatting, Incompatibility, and Dual-Purpose Space Specification

### A. Unicode Incompatibility Boundary
- **Compatible Range (`0x000000` to `0x10FFFF`):**
  - Consists of **District 0** (`0x000000`–`0x0FFFFF`) and the **first plane of District 1** (`0x100000`–`0x10FFFF`).
  - Code points in this zone MUST map 1:1 with standard UTF-8 / Unicode characters.
- **Incompatible Native Space (`0x110000` to `0x7FFFFFFF`):**
  - EVERYTHING above `0x10FFFF` is strictly **INCOMPATIBLE with standard Unicode**.
  - Standard UTF-8 parsers will treat these as illegal or out-of-range values. SUES treats them as native, unconstrained 31-bit OS primitives.

### B. Integrated Text Formatting System (Inline Control Code Points)
In SUCS, text formatting (bold, italics, text color, UI layout anchors) is NOT represented via external markup languages like HTML tags, ANSI escape sequences, or Markdown strings. It is handled directly via **native 31-bit formatting code points** reserved within the OS control space:

- Define explicit control constants in `include/superunicode/sucs_types.h`:
  ```c
  // Native SUCS Formatting & System Control Points
  #define SUCS_FMT_BOLD_ON      0x00110000
  #define SUCS_FMT_BOLD_OFF     0x00110001
  #define SUCS_FMT_ITALIC_ON    0x00110002
  #define SUCS_FMT_ITALIC_OFF   0x00110003
  #define SUCS_FMT_COLOR_RGB    0x00110010 // Followed by 24-bit color payload bytes
  #define SUCS_FMT_RESET        0x001100FF
  ```

### C. Dual-Purpose Extended Space (System Functions vs. Character Allocations)
Because SUCS is fundamentally a character encoding system, extended code points (`0x110000` to `0x7FFFFFFF`) serve a dual purpose: they can be interpreted either as **System Function Control Points** or as **Native Character Allocations.**

- Code Point Classifications:
    - `0x000000`–`0x10FFFF`: **Unicode Bridge Zone** (1:1 standard Unicode parity).
    - `0x110000`–`0x11FFFF`: **OS System Function & Formatting Control Space** (Inline renderer instructions, style shifts, and layout markers).
    - `0x120000`–`0x7FFFFFFF`: **Native SUCS Allocation Space** (Custom conlangs, RNUR multi-set allocations, neographies, technical symbols, and OS vector glyphs).

- API Requirements in include/superunicode/sucs_types.h & include/superunicode/sucs_plane.h:
    - Implement classification types and helper functions:
    ```c
    typedef enum {
        SUCS_TYPE_UNICODE_COMPAT, // 0x000000 - 0x10FFFF
        SUCS_TYPE_SYS_FUNCTION,   // 0x110000 - 0x11FFFF
        SUCS_TYPE_NATIVE_ALLOC    // 0x120000 - 0x7FFFFFFF
    } sucs_codepoint_type_t;
    sucs_codepoint_type_t sucs_classify_codepoint(sucs_char_t cp);
    bool sucs_is_unicode_compatible(sucs_char_t cp);
    bool sucs_is_formatting_char(sucs_char_t cp);
    ```

- Engine Logic Rules:
    - `SUCS_TYPE_SYS_FUNCTION` code points dictate state changes in the text rendering engine (e.g., color changes, weight toggles, inline metadata) and do **NOT** advance the visual text cursor position or count toward printable string width.
    - `SUCS_TYPE_NATIVE_ALLOC` code points represent printable glyphs allocated in custom planes and **DO** advance the text cursor position like standard character slots.

## 6. CRITICAL DEVELOPMENT & ISOLATION WARNING

> **IMPORTANT:** DO NOT TEST, RUN, OR ATTEMPT TO PARSE EXTENDED NATIVE SUCS CODEPOINTS (`> 0x10FFFF`) USING NATIVE WINDOWS APIS, STANDARD UNICODE RENDERING PIPELINES, OR HOST OS TEXT BOXES. 
> Standard UTF-8/Unicode parsers on Windows/Linux will treat 5-byte and 6-byte SUTF sequences as malformed/illegal byte streams and throw crashes, corruption, or unexpected behavior.
> Testing of raw SUTF streams is STRICTLY ISOLATED to byte-level CLI unit tests until the custom OpenWindows Kernel and display drivers are being built in their own standalone project.

---

## 7. PERSISTENT SYSTEM MEMORY ARTIFACT (HIDDEN STATE)

The agent must generate and maintain a hidden persistent state file named `.memory.md` (or `MEMORY.md` inside the hidden system-generated directory):

- **Path:** `./.memory.md` (or system/generated state directory)
- **Execution Rule (Silent File):** Keep this file updated silently in the background. **DO NOT** output, render, or print the contents of `.memory.md` into chat messages or terminal logs during builds or tests.
- **State Purpose:** Stores the complete architectural state (31-bit address space, SUTF 1–6 byte specs, `0x10FFFF` Unicode boundary, dual-purpose control/allocation logic, and kernel integration rules). 
- **Retrieval Protocol:** When instructed in future development phases (e.g., "Look for the memory file to build the kernel"), read `.memory.md` internally to restore the full project state before writing kernel code.

### Initial `.memory.md` Payload (Generated Silently):
```markdown
# SUPERUNICODE ARCHITECTURE STATE & MEMORY

## Core Boundaries & Rules
- Address Space: 31-bit unsigned (`0x00000000` to `0x7FFFFFFF`)
- SUTF Serialization: 1 to 6 bytes
- Unicode Parity Cutoff: `0x10FFFF` (District 0 & District 1, Plane 0)
- Native Extended Space: `0x110000` to `0x7FFFFFFF` (Strictly incompatible with standard Unicode / host OS text stacks)
- OS Target: OpenWindows (Non-POSIX, Non-NT, Capability-Based Architecture)

## Classification Space
- `0x000000`–`0x10FFFF`: Unicode Compatibility Zone
- `0x110000`–`0x11FFFF`: OS System Function & Inline Formatting Controls
- `0x120000`–`0x7FFFFFFF`: Native Character & Symbol Allocations

## Isolation Constraints
- DO NOT test extended SUTF streams using Windows/Linux native text controls or font stack APIs.
- Host testing is restricted to bit-level assertions in `./tests/`.
- Full rendering logic is deferred to the future OpenWindows kernel project.
```

---

## 8. CRITICAL CODING RULES FOR NATIVE EXTENDED PLANE ENCODING (SUTF 5-BYTE & 6-BYTE)

**Context:** Strict rules for C code generating 5-byte and 6-byte SUTF sequences for native extended SUCS codepoints (`0x00110000` through `0x7FFFFFFF`).

- **Unicode Limit:** `0x0010FFFF` (4-byte max).
- **Target Max:** `0x7FFFFFFF` (31-bit boundary).

---

### Rule 1: Strict SUTF Length Boundaries

SUTF serialization MUST dynamically assign byte counts based on bit capacity:
- **1 Byte:** `0x00000000`–`0x0000007F` (7 bits)  -> Header `0x00`
- **2 Bytes:** `0x00000080`–`0x000007FF` (11 bits) -> Header `0xC0`
- **3 Bytes:** `0x00000800`–`0x0000FFFF` (16 bits) -> Header `0xE0`
- **4 Bytes:** `0x00010000`–`0x0010FFFF` (21 bits) -> Header `0xF0` (Unicode Boundary)
- **5 Bytes:** `0x00110000`–`0x03FFFFFF` (26 bits) -> Header `0xF8` (`111110xx`)
- **6 Bytes:** `0x04000000`–`0x7FFFFFFF` (31 bits) -> Header `0xFC` (`1111110x`)

### Rule 2: 5-Byte Sequence Construction (Header `0xF8`)

For codepoints `0x00110000` to `0x03FFFFFF`, construct bytes as follows:
```c
out_buf[0] = 0xF8 | ((cp >> 24) & 0x03);
out_buf[1] = 0x80 | ((cp >> 18) & 0x3F);
out_buf[2] = 0x80 | ((cp >> 12) & 0x3F);
out_buf[3] = 0x80 | ((cp >> 6)  & 0x3F);
out_buf[4] = 0x80 | (cp         & 0x3F);
*out_bytes_written = 5;
```

### Rule 3: 6-Byte Sequence Construction (Header `0xFC`)

For codepoints `0x04000000` to `0x7FFFFFFF`, construct bytes as follows:
```c
out_buf[0] = 0xFC | ((cp >> 30) & 0x01);
out_buf[1] = 0x80 | ((cp >> 24) & 0x3F);
out_buf[2] = 0x80 | ((cp >> 18) & 0x3F);
out_buf[3] = 0x80 | ((cp >> 12) & 0x3F);
out_buf[4] = 0x80 | ((cp >> 6)  & 0x3F);
out_buf[5] = 0x80 | (cp         & 0x3F);
*out_bytes_written = 6;
```

### Rule 4: Out-Of-Bounds Validation

If codepoint > `0x7FFFFFFF`, immediately return `SUES_ERR_OUT_OF_BOUNDS`. DO NOT emit or truncate bits. The Kernel Security Trap range (`0x7FFFFFF0`–`0x7FFFFFFE`) and the in-band sentinel (`0x7FFFFFFF`) are reserved and must return `SUES_ERR_INVALID_CODEPOINT`.

### Rule 5: Buffer Allocation Checks

Before writing bytes, verify that the output buffer is sufficiently sized:
```c
if (cp >= 0x04000000) {
    if (buf_size < 6) return SUES_ERR_BUFFER_TOO_SMALL;
} else if (cp >= 0x00110000) {
    if (buf_size < 5) return SUES_ERR_BUFFER_TOO_SMALL;
}
```

### Rule 6: No UTF-8 Reuse on Extended Native Bytes

Extended native SUTF bytes must never be encoded using UTF-8 leading bytes like `0xF0`–`0xF4`. Those bytes belong exclusively to the standard Unicode 4-byte range. Native extended codepoints (`> 0x10FFFF`) MUST use the strict 5-byte (`0xF8`) / 6-byte (`0xFC`) templates above, with no partial UTF-8 patterns and no dropped bits.