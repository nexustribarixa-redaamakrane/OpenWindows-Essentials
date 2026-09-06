# Session Summary (OpenWindows-Essentials: Cairo/Pixman freestanding port)

## Objective
- Port the **actual genuine Cairo library** (cairo-1.18.2 + software rasterizer pixman-0.44.2) into OpenWindows-Essentials as three freestanding C99 OWD1 modules: `cairo64.owd` (Cairo core → ARGB32/RGB24 image surfaces), `cairoplus64.owd` (SUTF text via Cairo user-font), `userinterface64.owd` (compositor/controls). Adaptation strategy: interposition (k64-backed allocator + freestanding libc/libm symbol shims), not source rewrites.

## Important Details
- Architecture rules (still binding): freestanding C99 (`-std=c99 -ffreestanding -nostdlib`); no hosted libc in OUR code; OWD1 magic `0x4F574431`; memory via `k64_request_memory_block`; BANcode status protocol (B+ 0x0011A000–0x0011A77F, W+ 0x0011A800–0x0011ABFF, C+ 0x0011AC00–0x0011ADFF, S+ 0x0011AE00–0x0011AEFF; new modules must pick distinct range slices); vendor dirs never on module include path (modules consume `Extensions/` ABI mirrors; superunicode canonical path resolution).
- Kernel64 API: `k64_request_memory_block(size_t, void**)` (page-aligned + guard page, usable immediately), `k64_release_memory_block(void*)`, `k64_raise_system_exception(uint32_t bancode, uint64_t subcode)`, `k64_initialize_api()`; success `K64_OK`.
- Build strategy: vendored sources NOT edited; generated headers in `*/ow-build/` injected via `-iquote`/`-I`. Quoted includes (`"config.h"`, `"cairo-features.h"`) via `-iquote ow-build`; angle includes (`<pixman.h>`, `<pixman-config.h>`, `<pixman-version.h>`) need `-I`. Compilers: gcc/ar at `C:\w64devkit\bin\`.
- **Compiler gotcha**: this is GCC 15 (w64devkit), which made `-Wincompatible-pointer-types` a **hard error by default** (even with `-w`). Vendored builds need `-Wno-incompatible-pointer-types`.
- Config decisions: `CAIRO_NO_MUTEX=1`, `NDEBUG=1`, `CAIRO_DEBUG=0`; `HAVE_UINT64_T=1` (native cairo wideints instead of struct pair), `HAVE_CXX11_ATOMIC_PRIMITIVES=1` (GCC `__atomic_*`, inlined on x86-64); pixman flags `-DHAVE_CONFIG_H -DPIXMAN_NO_TLS` (PIXMAN_API is now defined by the version header itself). cairo `-DCAIRO_WIN32_STATIC_BUILD -DCAIRO_COMPILATION`.
- Vendored build flags: `-std=c99 -ffreestanding -nostdlib -O2 -w -Wno-incompatible-pointer-types -fno-stack-protector -fno-exceptions`. Our owrt TUs compile strict: `-Wall -Wextra -Werror -Wpedantic -fno-builtin`.
- cairo include line: `-iquote ow-build -I src -I pixman/pixman -I pixman/ow-build` (last two for `<pixman.h>` and `<pixman-version.h>`).
- `cairo-user-font.h` does NOT exist in cairo 1.18 (not shipped in tarball); the user-font API (create/setters/getters, `cairo_user_scaled_font_init_func_t`, etc.) is declared directly in `cairo.h` (lines 1802+ signatures, 2027 setters, 2049 getters; `cairo_user_font_face_create` returns `cairo_font_face_t *`). Modern meson generates no separate header; our modules must not expect one.
- `cairo_sources` in `vendor/cairo/src/meson.build` lines 1–119 (117 parsed entries used); feature sources (png/ft/xlib/xcb/quartz/win32/script/ps/pdf/svg/tee) all disabled via cairo-features.h (only IMAGE/RECORDING/MIME/OBSERVER/USER_FONT/UTF8_TO_UTF16=1).
- **Symbol audit method (reusable)**: link a probe TU that exercises the module's intended ABI with `gcc -Wl,-r` over [libcairo, libpixman, libowrt, libowrt] → `ld -r` pulls only reachable members, leaving true externals undefined; then `nm -u combined.o`. 
- **Audit result (clean)**: only 4 undefined — `k64_request_memory_block`, `k64_release_memory_block` (from owrt, resolved at module link), `__ImageBase` (PE-inherent at final link), `WinMain` (artifact of probe link's startup objects, absent with `-nostdlib`; NOT referenced by any source).
- The mass of raw-archive CRT/OS refs (`__mingw_fprintf`, `__imp_QueryPerformance*`, `__imp_localeconv`, `__imp_CreateFileW`, `__imp_*temp*`, `__imp__strdup`, `__imp___acrt_iob_func`, `_snprintf`, `longjmp` in scan converters, `___chkstk_ms`, `__popcountdi2`) all live in UNREACHABLE members for the intended ABI: cairo debug/output-stream/temp-file/time/tag-stack code and `CAIRO_DEBUG`-gated fprintf paths — none pulled by the module's image-surface + user-font + gstate entry set. So NO setjmp/longjmp, NO chkstk, NO win32 shims are currently needed. Re-run the probe whenever the ABI grows (e.g., if recording-surface or time-based features are added, `cairo-time`'s QueryPerformanceCounter and `_cairo_temp_file`'s CreateFileW chain WILL become live — plan shims then).

## Work State
### Completed
- SuperUnicode workstream delivered (Extensions compat layer, canonical vendoring, CMake ExternalProject `superunicode_vendor`, 9/9 ctest, 12/12 regression, `SUPERUNICODE_INTEGRATION.md`).
- Genuine cairo-1.18.2 + pixman-0.44.2 vendored; config headers: `vendor/cairo/ow-build/{cairo-features.h,config.h}`, `vendor/pixman/ow-build/{pixman-config.h,pixman-version.h}`.
- `pixman-version.h` FIXED to match the real template: `PIXMAN_VERSION` is now numeric (`PIXMAN_VERSION_ENCODE(major,minor,micro)`), `PIXMAN_VERSION_STRING` the string, plus `#ifndef PIXMAN_API` guard → `-DPIXMAN_API=` flag removed from cpixman.ps1.
- Build drivers `cpixman.ps1`, `ccairo.ps1` (per-file diagnostics are swallowed by `| Out-Null` — probe failing files manually), `crotrt.ps1`.
- **pixman 31/31** → `vendor/pixman/ow-build/build/libpixman_vendor.a`.
- **cairo core 117/117** → `vendor/cairo/ow-build/build/libcairo64_vendor.a` (fixes applied: `-I pixman\ow-build`; config.h additions HAVE_UINT64_T + HAVE_CXX11_ATOMIC_PRIMITIVES; `-Wno-incompatible-pointer-types`).
- Freestanding owrt shim (all strict-profile clean) → `DLL/cairo64/owrt/build/libowrt64.a`: `ow_runtime.h` (declared symbol set), `ow_alloc.c` (segregated free-list malloc/slab 16–8192, 16-byte header at user_ptr−16, large = dedicated k64 block), `ow_str.c` (+qsort), `ow_fmt.c` (snprintf/vsnprintf + strtol family + strtod), `ow_math.c` (libm subset).
- Relocatable link probe (probe_core.c exercises create/source/paint/stroke/fill/user-font/select-font-family/show-text/show-glyphs/flush/get-data): LINK clean; unresolved = k64 pair + `__ImageBase` (WinMain artifact excluded via `-nostdlib` at real link).

### Active
- Next: implement the OWD1 module `DLL/cairo64/` (`cairo64.owd`): OWD1 magic header + export table binding `cairo_*` public API (image surface, recording surface, gstate paint/stroke/fill path ops, user-font + toy font entry points, getters/data), BANcode ranges, k64 memory init + `_cairo_*` statics reset, and the interposition assembly at link.
- Then `cairoplus64.owd` (SUTF text via user-font/scaled-font plumbing), then `userinterface64.owd` (compositor).
- CMake targets for the three modules + 12-module strict regression; re-run probe augmentation as ABI grows.

### Blocked
- Nothing currently blocked; `setjmp`/`longjmp` + `___chkstk_ms` + QPC/CreateFileW shims deferred until/unless a real module link demands them (no evidence yet).

## Next Move (resume point)
1. Inspect an existing completed OWD1 module (e.g., `DLL/suct/`) + `Extensions/owd_format.h` for the exact header/export/status conventions.
2. Write `DLL/cairo64/` module: entry point, memory init via `k64_request_memory_block`, `cairo64.owd` export set for the ABI surface used by probe_core (validated clean), link with `-nostdlib` + `--no-undefined` to enforce the audit.
3. Verify: link proves only k64 + `__ImageBase` externals; then smoke-test rendering (ARGB32 paint/fill/stroke → inspect data).
4. cairoplus64.owd (SUTF text), userinterface64.owd, CMake wiring, strict regression.

## Relevant Files
- `vendor/pixman/ow-build/pixman-version.h` + `cpixman.ps1` → `build/libpixman_vendor.a` (31/31).
- `vendor/cairo/ow-build/{config.h,cairo-features.h,ccairo.ps1}` → `build/libcairo64_vendor.a` (117/117).
- `vendor/cairo/src/meson.build` (cairo_sources lines 1–119); `vendor/pixman/pixman/pixman-version.h.in` (template source of truth).
- `DLL/cairo64/owrt/`: `ow_runtime.h`, `ow_alloc.c`, `ow_str.c`, `ow_fmt.c`, `ow_math.c`, `crotrt.ps1`, `build/libowrt64.a`.
- Probe (temp, reusable): `%TEMP%\opencode\probe_core.c` + `probe_link.ps1` → `combined.o` (4 undefined: k64 pair, __ImageBase, WinMain).
- `DLL/kernel64/kernel64.h` (k64 API), `Extensions/` (`owd_format.h`, sucs/sutf), `DLL/suct/`, `DLL/owrp/owrp.h` (OWD1 + cairoplus64 binding conventions).