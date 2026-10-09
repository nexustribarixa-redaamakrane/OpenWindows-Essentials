# OpenWindows Native Executable (.owx) Binary Format Specification

## 1. Overview
The `.owx` format defines the native binary execution standard for userland applications, console utilities, system daemons, and windowed programs in OpenWindows.

## 2. Binary Layout
Every `.owx` binary begins with a fixed 256-byte header naturally aligned on 8-byte boundaries:

```
+-------------------------------------------------------------+
| owx_header_t (256 bytes)                                    |
|   - Magic: 0x4F575831 ("OWX1")                              |
|   - Format Version: 0x0001                                  |
|   - Entry Point, Preferred Base                             |
|   - Stack Reserve/Commit, Heap Reserve/Commit               |
|   - Subsystem ID, Architecture ID                           |
|   - Table Offsets & Checksums                               |
+-------------------------------------------------------------+
| Section Table (32 bytes * section_count)                    |
|   - .code, .rdata, .data, .bss, .reloc, .import, .tls...    |
+-------------------------------------------------------------+
| Import Table (32 bytes * import_count)                      |
|   - .owd dynamic library dependencies & checksums           |
+-------------------------------------------------------------+
| String Table                                                |
+-------------------------------------------------------------+
| Section Payloads (aligned to 4096-byte pages)               |
+-------------------------------------------------------------+
```

## 3. Subsystem Constants
- `0x01`: Native Kernel Service
- `0x02`: Character Console / Shell
- `0x03`: Cairo / OWUI Windowed Application
- `0x04`: Boot Init Utility
- `0x05`: Sentinel Crash Recovery Tool

## 4. Zero-Allocation & Freestanding Principles
The loader allocates static virtual memory ranges based on `stack_reserve` and `heap_reserve`. No runtime heap expansion is performed outside predefined caller pools.
