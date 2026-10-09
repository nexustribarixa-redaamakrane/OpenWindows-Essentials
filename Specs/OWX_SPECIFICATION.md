# OWX Native Executable Format Specification

## Overview
The .owx format is the native executable binary format for OpenWindows.

## Header Layout
| Offset | Size | Field |
|--------|------|-------|
| 0x00 | 4 | Magic (0x4F575831) |
| 0x04 | 4 | Version |
| 0x08 | 4 | Flags |
| 0x0C | 4 | Entry Point |
| 0x10 | 4 | Section Count |
| 0x14 | 4 | Import Count |
| 0x18 | 4 | Export Count |
| 0x1C | 8 | Image Size |
| 0x24 | 4 | Stack Size |
| 0x28 | 4 | Heap Size |
| 0x2C | 4 | Checksum |
| 0x30 | 4 | Subsystem |
| 0x34 | 204 | Reserved |

Total header: 256 bytes

