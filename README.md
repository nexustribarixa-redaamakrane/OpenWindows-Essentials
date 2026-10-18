# OpenWindows-Essentials

OpenWindows-Essentials is a freestanding C collection of OpenWindows kernel
components, driver modules, dynamic libraries, native executables, binary
format definitions, and vendored dependencies. Its top-level
`CMakeLists.txt` links targets into OpenWindows-specific PE files:

- `.owc` driver/component images, using `Drivers/owc_entry.c`;
- `.owd` libraries, using `DLL/owd_entry.c`;
- `.owx` user programs, using `Software/owx_entry.c`.

These suffixes and entry stubs are project conventions, not standard Windows
file formats. The format declarations are in `Extensions/` and the detailed
specifications are in `Specs/`.

## Contents

- `Drivers/` — device, bus, input, storage, filesystem, and filter components.
- `DLL/` — kernel/runtime interfaces and libraries, including HTL, OWRP,
  boot video, kernel, scheduler, memory-management, networking, and graphics
  components.
- `Runtimes/` — the freestanding C runtime.
- `Software/` — `owinit` and utilities such as `owsh`, `owedit`, `owkill`,
  `owwm`, `vipmount`, `sfontview`, and control tools.
- `Extensions/`, `Artifacts/Extensions/`, `Specs/` — interfaces, binary
  layouts, and their specifications.
- `vendor/` — third-party sources used by selected targets, including Cairo
  and Pixman.

## Build

The top-level build requires CMake 3.20+ and a GCC-compatible compiler/linker
that accepts the freestanding and PE linker options in `CMakeLists.txt`
(normally MinGW-w64 GCC). From the repository root:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_C_COMPILER=gcc
cmake --build build --parallel 2
```

Outputs are grouped under `build/Drivers`, `build/DLL`, and `build/Software`.
The CMake file declares the component targets and pulls `libgcc` into the
freestanding links; a hosted C runtime is not linked as an application
dependency. Some vendor targets have their own PowerShell build scripts under
`vendor/`.

There is no CTest suite registered by the top-level CMake file. The repository
does contain targeted scripts, including `audit_modules.ps1`, and component
directories have local test/smoke scripts; run the script belonging to the
module you are changing.

## Implementation status

The repository contains a broad set of build targets, but a target existing
does not mean its hardware behavior is complete. Several low-level drivers
still contain explicit TODO stubs; examples include `Drivers/ahci/ahci.c`,
`Drivers/atapio/atapio.c`, and `Drivers/devnull/devnull.c`. The filesystem
driver directories also include shared scaffold-style implementations. Check
the relevant `.c` file before treating a target as usable on hardware.

In CMake, every `.owx` target names its own real entry point, and
`ow_owx_props()` fails configuration if one is omitted, so no program can
silently fall back to the inert `owx_entry_stub` (`xor eax,eax; ret`). The
eight targets that used to keep the stub — `kextctl`, `owedit`, `owkill`,
`owwm`, `sfontview`, `startwm`, `vipmount`, and `win` — now enter their own
`*_main`, reach the kernel only through the Ring 3 gateway in
`Software/owx_gate.h`, and link their window-management, task-manager,
sentinel, and font-rendering libraries as source rather than as `.owd`, since
the OWX1 loader binds no imports. The kernel repository audits all thirteen
staged artifacts (`tools/check_owx_entries.py`, run by `make owx-entries`)
against the properties that matter: import-free, not the stub, and for
`owinit` the pinned `owinit_main` prologue at RVA 0x1000. Note that entering
at a real function is not the same as a finished userspace: the gateway today
exposes console I/O, `VFS_READ`, and process services, so e.g. `owedit` can
load and edit a document but cannot write it back. No physical-hardware
support matrix is maintained here.

For the file contracts, use `Specs/`; for executable entry and object details,
use the corresponding CMake target and source under `Drivers/`, `DLL/`, or
`Software/`. `SUPERUNICODE_INTEGRATION.md` describes the local integration
assumptions.
