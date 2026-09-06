# OpenWindows Hierarchical Configuration (.kconf) Specification

## 1. Overview
The `.kconf` engine provides a structured, human-readable, Linux-like configuration tree acting as the OpenWindows counterpart to the Windows Registry.

## 2. Key-Value Path Addressing
Keys are case-insensitive dot/slash paths:
- `kernel.panic.action`: Action taken upon BANcode fatal error (`halt`, `reboot`, `sentinel`).
- `drivers.sio.baud_rate`: Baud rate for COM1 16550 UART character device.
- `ui.wm.theme`: Active color theme for the `owwm` window manager.

## 3. Storage & Zero-Allocation Access
Config files are stored as UTF-8 plaintext during editing and compiled into `.kconf` hybrid cache files for O(1) hash table lookup at boot time. All API queries (`kconf_lookup()`) operate within static pre-allocated node pools.
