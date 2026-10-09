# SuperUnicode workspace

This C99 workspace contains several related encoding, text-processing,
serialization, and font-container libraries. It is an experimental project
API and format suite; its names do not imply adoption by Unicode or
interoperability with an external standard.

## Build modules

| Module | CMake target | Source area | Purpose in this tree |
| --- | --- | --- | --- |
| Base SUCS | `superunicode_static` | `superunicode/` | 32-bit code-point API, SUTF encode/decode helpers, strings, conversion, trap helpers, and bundled UCD names |
| Base SUTF | `sutf_static` | `sutf/` | SUTF-8, SUTF-16, SUTF-4, SUTF-2, and mode code |
| Extended SUCS | `superunicode_extended_static` | `superunicode_extended/` | 64-bit character values, vSUTF, and conversions |
| SUST | `sust_static` | `sust/` | SUST-16, fixed-width transports, and e-SUST |
| SUF | `suf_static` | `suf/` | SUF parser, builder, and conversion code |
| SUAS | `suas_static` | `src/suas/` | Directional framing, glyph-width, line-boundary, and canonical-form code |
| SUTS | `suts_static` | `src/suts/` | SUCA collation key generation/comparison |
| Plugin support | `sucs_plugin_static` | `superunicode_extended/plugin/` | Plugin lifecycle and packaging support |

Base `sucs_char_t` is a `uint32_t`; the extended text-processing APIs use
`sucs_ex_char_t` as `uint64_t`. The base library publishes Unicode 18.0 UCD
metadata in `superunicode/include/superunicode/`. SUTF is a transformation
layer in this codebase; SUST defines byte/storage transport details. Check the
module headers and specifications under `include/` and `docs/` for exact
contracts.

The `suf/tools/` CMake targets include `suf_inspector`, conversions from
TrueType/OpenType/WOFF/FontForge SFD/EOT/PostScript/PFB/UFO into SUF, and
conversions back to those named formats. These are project tools and formats;
round-trip fidelity and complete coverage of every feature in each source
format are not guaranteed by the target names.

## Configure, build, and test

Requirements: CMake 3.10+ and a C99 compiler. The static libraries are built
with freestanding flags; the test executables use the host C runtime.

```powershell
cmake -S . -B build -DCMAKE_C_COMPILER=gcc
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The registered CTest suite covers base encoding/conversion/UCD checks, SUTF,
extended SUTF/conversions/UCD, plugin lifecycle, SUST, SUF, SUAS/SUTS, and
unified-header coexistence. Several module CMake files place executables and
archives in their own `bin/` directories under the source tree.

The documentation scaffold check is a separate target:

```powershell
cmake --build build --target verify-docs
```

It checks documentation-file presence; it is not an encoding conformance
test.

## Repository map

- `include/`, `superunicode/`, `sutf/`, `superunicode_extended/`, `sust/`,
  `suf/` — public headers, implementation, and module-specific tools.
- `src/suas/`, `src/suts/` — root-level text-processing implementations.
- `tests/`, module `tests/`, `unified/` — CTest inputs.
- `compat/` — compatibility copies and smoke tests for sibling projects.
- `docs/` — format and architecture specifications.
- `website/` — static documentation site.

Treat format support and Unicode behavior as implementation-specific until
verified against the corresponding tests and specification. The CMake suite
does not certify conformance to Unicode algorithms or support for every
platform/font feature.
