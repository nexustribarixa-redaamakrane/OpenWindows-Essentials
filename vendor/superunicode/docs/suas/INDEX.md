# SUAS — SuperUnicode Architecture Standards

> Core Kernel Invariants

This directory contains the formal architecture standards governing the
SuperUnicode kernel internals. SUAS documents define the invariant contracts,
memory models, and encoding guarantees that all conforming implementations
must uphold.

## Document Index

| ID | Title | Status |
|----|-------|--------|
| [SUAS-001](SUAS-001-sdf.md) | Structural Directional Framing (SDF) — Core BiDi Architecture | Draft — Ratified |
| [SUAS-002](SUAS-002-sgw.md) | System Glyph Width & Monospace Grid (SGW) — Fixed-Cell Terminal Metrics | Draft — Ratified |
| [SUAS-003](SUAS-003-sbr.md) | System Boundary & Line Break Rules (SBR) — Single-Pass Line Breaking | Draft — Ratified |
| [SUAS-004](SUAS-004-sucf.md) | SuperUnicode Canonical Forms (SUCF) — Dual-Target Canonical (De)composition | Draft — Ratified |

## Scope

- Kernel invariant definitions
- Memory layout and alignment contracts
- Codepoint representation guarantees
- Conformance requirements for implementations
- Bi-directional layout, scope isolation, and glyph mirroring
- Single-byte / fixed-cell width metrics and monospace grid layout
- Line-boundary resolution and system-controlled break directives
- Canonical equivalence: dual-target canonical (de)composition over 64-bit SUCS

## Status

**SUAS-001 (SDF), SUAS-002 (SGW), SUAS-003 (SBR) and SUAS-004 (SUCF) are
ratified.** Additional SUAS rules and invariants are pending authoring.
