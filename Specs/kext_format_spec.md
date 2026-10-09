# OpenWindows Kernel Extension (.kext) Specification

## 1. Overview
Kernel Extensions (`.kext`) are modular ring-0 plugins that attach to kernel subsystems without recompiling the microkernel core.

## 2. Header Format
- Fixed 192 bytes, magic `0x4B455854` (`KEXT`).
- Contains hook table offsets, init/fini entry points, and domain ID:
  - `0x01`: Storage (VIP/OWFS)
  - `0x02`: Networking
  - `0x03`: Memory & VMM
  - `0x04`: BANcode / Sentinel
  - `0x05`: Framebuffer / Cairo UI

## 3. Hook Dispatch
Hooks are executed according to `priority` (0 = highest). Callbacks take typed caller context pointers with zero heap allocations.
