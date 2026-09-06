# OpenWindows Hybrid Cache (.vcache, .symcache, .fcache) Specification

## 1. Concept
A hybrid cache pairs a fixed 128-byte binary header with an array of indexed bucket records, followed by payload sectors. This ensures atomic verification, direct DMA mapping, and zero-allocation parsing.

## 2. File Types
- `.vcache`: Volume Index Partition sector bucket cache for fast FVIP directory traversal.
- `.symcache`: Kernel symbol and stack unwinding hash table for instant panic backtraces.
- `.fcache`: Pre-rendered font glyph bitmap cache for SuperUnicode character rendering.
- `.kconf`: Pre-indexed configuration tree cache.
