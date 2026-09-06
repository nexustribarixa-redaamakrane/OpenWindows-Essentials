# SuperUnicode Integration Contract

OpenWindows-Essentials integrates the canonical **SuperUnicode** workspace
(base SUCS + SUTF transformation formats, SUST serialization transports, the
.suf font transport, and the BANcode Kernel Security Trap registry) at two
layers:

1. **`vendor/superunicode/`** — a pinned mirror of the canonical workspace
   (`C:\Users\KARIMABENDA\Documents\superunicode`). Self-contained CMake
   project, built and tested in isolation.
2. **`Extensions/`** — a freestanding C99 *compat layer* of self-contained
   static-inline headers that mirror the canonical constants, transports,
   mode-switching and trap-dispatch contracts, and are what the OpenWindows
   `.owc`/`.owd` modules actually include.

The vendor tree is **never** on the module include path and is **never** linked
into any OpenWindows module. It is the authoritative reference implementation
and test bed; `Extensions/` is the header-only, freestanding drop-in.

---

## Vendor Location & Scope

`vendor/superunicode/` mirrors the canonical workspace source tree:

| Entry                | Contents                                                      |
|----------------------|---------------------------------------------------------------|
| `superunicode/`      | Base SUCS core: SUTF encode/decode, `sucs_string`, `sucs_trap`, `sucs_conv`, UCD names |
| `sutf/`              | Base and extended transformation formats: `sutf8/16/4/2`, `vsutf`, mode/type forwarders |
| `sust/`              | SUST serialization transports: `sust16` (+ explicit `_be`/`_le`), fixed-width `sust32/64/128/256/512/N`, `esust` page-mapped e-SUST |
| `superunicode_extended/` | ExtSUCS: `extsucs_types`, vSUTF transformation; plugin subsystem |
| `suf/`               | SuperUnicode Font (.suf) parser/builder + conversion tools      |
| `unified/`           | Single-TU header-coexistence test (all headers in one TU)       |
| `compat/`            | Ecosystem compat sources (BANcode, bootloader, storage, vip). Not wired into the root build |
| `CMakeLists.txt`     | Canonical root; builds all wired sub-projects via `enable_testing()` |

The vendor tree is synced from the canonical workspace as a git clone (branch
`main`); since the canonical repo **does** commit `website/` and `compat/`, the
clone carries them too. The vendor `build/` products and `compat/`'s own CMake
(which references sibling repos not present here) are excluded from the root
build — `compat/` is not wired into the vendor CMake and `website/` is build
output only, never on any module include path.

### Regenerating a fresh vendor copy

```powershell
robocopy C:\Users\KARIMABENDA\Documents\superunicode `
    C:\Users\KARIMABENDA\Documents\OpenWindows-Essentials\vendor\superunicode `
    /E /XD build website .vscode /XF *.exe *.o *.a *.obj *.lib *.pdb *.ilk *.map
```

---

## Building & Testing the Canonical Workspace

Standalone (direct, w64devkit GCC):

```powershell
cmake -S vendor/superunicode -B build-vendor/superunicode `
      -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
cmake --build build-vendor/superunicode -j
ctest --test-dir build-vendor/superunicode --output-on-failure
```

Wired into the OpenWindows root (isolated ExternalProject, on by option):

```powershell
cmake -S . -B build -DOW_BUILD_VENDOR_SUPERUNICODE=ON
cmake --build build --target superunicode_vendor   # configures, builds, runs ctest
```

The vendor project uses a **hosted** toolchain (its tests/tools link the CRT).
It is deliberately not `add_subdirectory()`-ed: the OpenWindows global flags
(`-ffreestanding -nostdlib -Werror`) would break its hosted test targets.

Expected results: **10/10 tests pass** — `test_sutf`, `test_sucs_planes`,
`test_conv`, `test_sutf_all`, `test_sust_all`, `test_extsutf_all`,
`test_extconv`, `test_plugin_lifecycle`, `test_suf`, `test_unified`.

---

## Extensions Compat Layer

All headers are C99-freestanding, `stdint`/`stdbool`/`stddef`-only, static
inline, zero-allocation, and `#include`-safe in any order.

| `Extensions/` header | Canonical counterpart | Mirrors |
|----------------------|----------------------|---------|
| `sucs_types.h`       | `sutf/include/sucs_types.h` | `sucs_char_t`, `SUCS_INVALID_CODEPOINT`, `SUCS_MAX_CODEPOINT`, `sucs_is_valid`, `sucs_kernel_mode_t` (guarded) |
| `sutf8.h`            | `sutf/include/sutf8.h`      | SUTF-8 1..6-byte transformation |
| `sutf16.h`           | `sutf/include/sutf16.h`     | SUTF-16 1..2-word transformation (endian-neutral; byte I/O via SUST-16) |
| `sutf4.h`            | `sutf/include/sutf4.h`      | SUTF-4 8-nibble / 4-byte transformation |
| `sutf2.h`            | `sutf/include/sutf2.h`      | SUTF-2 16-frame / 4-byte (32-bit) transformation |
| `sust16.h`           | `sust/include/sust16.h`     | SUST-16 byte serialization, explicit big- and little-endian variants |
| `sucs_mode.h`        | `sutf/include/sucs_mode.h` + `sutf/src/sucs_mode.c` | `sucs_kernel_mode_t`, `sucs_switch_status_t`, `sucs_kernel_boot_config_t`, request/commit/init |
| `sucs_trap.h`        | `superunicode/include/superunicode/sucs_trap.h` + `src/sucs_trap.c` | Registry constants, classifiers, BANcode↔trap mapping, dispatch table, diagnostics, and the **BANcode System/App operating-mode surface** (v1.1.0): `sucs_bancode_mode_t`, `SUCS_BANCODE_DEFAULT_MODE`, `sucs_bancode_app_crash_handler_t`, mode get/set + app-crash-handler register/unregister |
| `sutf.h`             | `sutf/include/sutf.h`        | Umbrella: types + mode + trap + sutf8/16/4/2 + sust16; forwards vSUTF/SUST headers via `__has_include` |

### Shared Constants (identical in both layers)

| Constant | Value |
|----------|-------|
| `SUCS_MAX_CODEPOINT` / `SUCS_INVALID_CODEPOINT` | `0x7FFFFFFFUL` |
| `SUCS_KERNEL_TRAP_MIN` / `MAX` | `0x7FFFFFF0UL` / `0x7FFFFFFEUL` |
| `SUCS_SCP_MIN` / `MAX` | `0x00110000UL` / `0x0011FFFFUL` |
| `SUCS_BANCODE_RANGE_MIN` / `MAX` | `0x0011A000UL` / `0x0011A7FFUL` |
| `SUCS_WARNCODE_RANGE` | `0x0011A800` – `0x0011ABFF` |
| `SUCS_COMCODE_RANGE` | `0x0011AC00` – `0x0011ADFF` |
| `SUCS_SOFTCODE_RANGE` | `0x0011AE00` – `0x0011AEFF` |
| `SUCS_BANCODE_REGISTRY_MIN` / `MAX` | `0x0011A000UL` / `0x0011AEFFUL` |
| `SUCS_TRAP_SLOT_COUNT` | `15` |
| `SUCS_BANCODES_PER_TRAP` | `128` |

These equal `owc_format.h`'s trap geometry (`OWC_BANCODE_RANGE_LO/HI`,
`OWC_TRAP_BASE/END`, `OWC_TRAP_SLOT_COUNT 15`).

### Kernel Security Trap geometry & dispatch

* Slot `n` → trap codepoint `0x7FFFFFF0 + n` (0..14).
* Slot `n` governs B+ BANcodes `0x0011A000 + n*128` .. `+127`.
* `sucs_bancode_to_trap(cp)` resolves B+ BANcode → trap; non-B+ or cluster
  beyond slot 14 (i.e. `0x0011A780`+) → `SUCS_INVALID_CODEPOINT`.
* `sucs_trap_dispatch(bancode)` invokes the installed handler for its slot and
  records a `sucs_trap_diagnostic_t`; non-fatal BANcodes, unmapped codes and
  empty slots return `false`.
* **BANcode operating modes (v1.1.0):** in the default **System mode**
  (`SUCS_BANCODE_DEFAULT_MODE == SUCS_BANCODE_MODE_SYSTEM`) fatal B+ BANcodes
  route to the Kernel Security Trap table as above. In **App mode**
  (`sucs_bancode_set_mode(SUCS_BANCODE_MODE_APP)`), fatal B+ BANcodes bypass
  the trap table and are delivered to the registered App-level crash handler
  (`sucs_bancode_register_app_crash_handler`). Both modes share the identical
  BANcode registry.
* Divisibility/mapping cases verified against the canonical registry test
  (`compat/test_compat_bancode.c`): `0x0011A000→0x7FFFFFF0`,
  `0x0011A080→0x7FFFFFF1`, `0x0011A3E6→0x7FFFFFF7`, `0x0011A77F→0x7FFFFFFE`,
  `0x0011A780→invalid`.

### Kernel mode-switching semantics

* Base (`0`) and Extended (`1`) are staged; a switch never applies
  immediately.
* `sucs_request_mode_switch()` stages `pending_mode` + `reboot_required` and
  returns `SUCS_SWITCH_REBOOT_REQUIRED`; re-requesting the active mode without
  a pending reboot returns `SUCS_SWITCH_ERR_ALREADY_ACTIVE`.
* `sucs_commit_mode_on_boot(config)` — called during early boot — promotes
  `pending_mode`→`active_mode` only when `reboot_required` is set, then clears
  the flag and bumps `mode_change_count`.
* `NULL` config targets the (per-TU) internal boot config.

### Coexistence rules

* Constants and typedefs are `#ifndef`-guarded (`SUCS_TRAP_SLOT_COUNT`,
  `SUCS_KERNEL_TRAP_MIN`, `SUCS_BANCODE_TYPE_T_DEFINED`,
  `SUCS_KERNEL_MODE_T_DEFINED`, `SUCS_SWITCH_STATUS_T_DEFINED`,
  `SUCS_KERNEL_BOOT_CONFIG_T_DEFINED`, …) so canonical and Extensions headers
  may share one translation unit.
* Function names are **static inline** in `Extensions/` and **extern** in the
  canonical libs — a TU must include only one layer's function definitions.
* Dispatch table, diagnostics, and boot config are **file-scope static in the
  header**, i.e. per-translation-unit state. Link the canonical
  `superunicode_static` / `sutf_static` (or a future `.owd`) when shared
  cross-TU state is required; `Extensions/` state is documented as per-TU.
* File-scope statics are annotated `OWE_SUCS_UNUSED` /
  `OWE_SUCS_MODE_UNUSED` (`__attribute__((unused))`) so a bare include never
  trips `-Wunused-variable` under `-Werror`.

---

## Module BANcode Assignments vs Canonical Registry

New modules occupy canonical registry zones and are dispatchable/crash-manageable
through `sucs_trap_dispatch()`:

| Module | B+ (FATAL) | W+ (WARN) | S+ (SOFT) |
|--------|-----------|-----------|-----------|
| `suct.owd` (SuperUnicode Text Services) | `0x0011A1C0`+ | `0x0011A946` | `0x0011AE90`+ |
| `sio.owc` (16550 UART) | `0x0011A2E0`+ | `0x0011A940`+ | `0x0011AE80`+ |
| `vipm.owc` (Volume Index Manager) | `0x0011A380`+ | `0x0011AA80`+ | `0x0011AEC0`+ |
| format/sentinel violations (`owc_format.h`) | `0x0011A000`+ | – | – |

## Regression

Recompile the compat layer (bare include + exercised TU) and every module under
the strict freestanding profile:

```powershell
$base="C:\Users\KARIMABENDA\Documents\OpenWindows-Essentials"
$inc = @(
  "$base\Extensions", "$base\Extensions\damage_control",
  "$base\DLL\owrp", "$base\DLL\kernel64", "$base\DLL\htl",
  "$base\DLL\bootvid", "$base\DLL\suct",
  "$base\Drivers\fltrmgr", "$base\Drivers\classpnp",
  "$base\Drivers\banhammer", "$base\Drivers\sio", "$base\Drivers\vipm"
)
$incArgs = ($inc | ForEach-Object { "-I`"$_`"" }) -join " "
$flags = "-std=c99 -ffreestanding -nostdlib -Wall -Wextra -Werror -Wpedantic -fsyntax-only"
foreach ($src in (Get-ChildItem -Recurse "$base" -Filter *.c |
                  Where-Object { $_.FullName -notlike "*\vendor\*" })) {
  gcc $flags.Split(' ') $incArgs "`"$($src.FullName)`"" 2>&1 | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "FAIL: $src" }
}
```