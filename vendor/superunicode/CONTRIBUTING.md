# Contributing to SuperUnicode

Thank you for your interest in contributing to SuperUnicode! This project implements the SUCS/ExtSUCS character encodings, SUTF/extSUTF transformation formats, and SUST serialization transports — foundational components of the OpenWindows Operating System.

---

## Table of Contents

- [Getting Started](#getting-started)
- [Development Environment](#development-environment)
- [Project Structure](#project-structure)
- [Code Standards](#code-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Reporting Issues](#reporting-issues)
- [License](#license)

---

## Getting Started

1. **Fork** the repository on GitHub.
2. **Clone** your fork locally:
   ```bash
   git clone https://github.com/nexustribarixa-redaamakrane/SuperUnicode.git
   cd SuperUnicode
   ```
3. **Create a branch** for your work:
   ```bash
   git checkout -b feature/your-feature-name
   ```

---

## Development Environment

### Prerequisites

- **C Compiler**: GCC or MinGW with C99 support
- **CMake**: Version 3.10 or higher
- **Make**: MinGW Make, GNU Make, or Ninja

### Building

```bash
# Configure
cmake -B build -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc

# Build all libraries and tests
cmake --build build

# Run the test suite
ctest --test-dir build --output-on-failure
```

---

## Project Structure

```text
SuperUnicode/
├── superunicode/              # Base SUCS encoding & inspector tools
├── sutf/                      # Bare-metal SUTF transports (libsutf.a)
└── superunicode_extended/     # ExtSUCS 64-bit & extSUTF transports
```

Each sub-project follows the same layout:
- `include/` — Public headers
- `src/` — Implementation files
- `tests/` — Unit tests

---

## Code Standards

### Strict Freestanding C99

All library code (`src/` and `include/`) **must** compile under:

```
-std=c99 -nostdlib -ffreestanding
```

This means:

- ✅ **Allowed headers**: `<stdint.h>`, `<stdbool.h>`, `<stddef.h>` (compiler-provided)
- ❌ **Forbidden**: `<stdio.h>`, `<stdlib.h>`, `<string.h>`, or any libc header
- ❌ **No dynamic allocation**: `malloc`, `calloc`, `realloc`, and `free` are prohibited
- ❌ **No global mutable state** unless explicitly required by the kernel mode subsystem

### Style Guidelines

- **Naming**: `snake_case` for functions and variables, `UPPER_SNAKE_CASE` for macros and constants
- **Prefixes**: All public symbols must use `sucs_`, `sutf_`, `vsutf_`, `sust_`, or `esust_` prefixes
  - `sucs_` / `sutf_` / `vsutf_` for character encodings and transformation formats
  - `sust_` / `esust_` for serialization transports (SUST library)
- **Comments**: Use `/* C89-style block comments */` for compatibility. Document all public API functions with a brief description, parameter list, and return value
- **Indentation**: 4 spaces, no tabs
- **Line length**: 120 characters max

### Test Code Exception

Code inside `tests/` and `tools/` **may** use standard library headers (`<stdio.h>`, `<assert.h>`, etc.) since tests run on the host development system.

---

## Testing

Every change must pass the existing test suite before submission:

```bash
ctest --test-dir build --output-on-failure
```

### Writing Tests

- Place test files in the appropriate `tests/` directory of the sub-project you're modifying
- Use `assert()` for test assertions
- Test edge cases: boundary codepoints (`0x7F`, `0x7FF`, `0xFFFF`, `0x10FFFF`, `0x110000`, `0x7FFFFFFF`), buffer overflow scenarios, and the kernel security trap range (`0x7FFFFFF0`–`0x7FFFFFFE`)

### ⚠️ Critical Isolation Rule

**DO NOT** test extended SUCS codepoints (`> 0x10FFFF`) using native OS text APIs, Unicode rendering pipelines, or GUI text fields. Standard UTF-8 parsers will reject 5-byte and 6-byte SUTF sequences. All testing must be byte-level assertions only.

---

## Submitting Changes

### Pull Request Process

1. Ensure your code **builds cleanly** with no warnings under `-Wall -Wextra -Werror`
2. Ensure **all tests pass**
3. Add tests for any new functionality
4. Update documentation if you change public APIs
5. Commit with clear, descriptive messages:
   ```
   sutf: add SUTF-4 nibble boundary validation
   
   Adds range checks for 4-bit nibble extraction to prevent
   silent truncation on malformed input streams.
   ```
6. Push to your fork and open a **Pull Request** against `main`

### Commit Message Format

```
<component>: <short description>

<optional longer explanation>
```

Components: `sucs`, `sutf`, `sust`, `vsutf`, `esust`, `cmake`, `docs`, `tests`

### What We Look For

- Adherence to freestanding C99 constraints
- No new compiler warnings
- Tests covering both happy paths and error cases
- Clean, readable code with appropriate comments

---

## Reporting Issues

When filing an issue, please include:

- **Description**: What happened vs. what you expected
- **Environment**: OS, compiler version, CMake version
- **Reproduction steps**: Minimal code or commands to reproduce
- **Component**: Which sub-project is affected (`sutf`, `superunicode`, `superunicode_extended`)

---

## License

By contributing to SuperUnicode, you agree that your contributions will be dual-licensed under:

- **MIT License** ([LICENSE-MIT](LICENSE-MIT))
- **Apache License, Version 2.0** ([LICENSE-APACHE](LICENSE-APACHE))

at the maintainer's option.
