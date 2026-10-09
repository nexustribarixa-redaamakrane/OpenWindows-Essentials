# SUTR-2 — SuperUnicode Collation Algorithm (SUCA)

**Standard ID:** SUTR-2
**Status:** 0.1.0 — Published
**Superseded By:** SUTS-001 (normative)
**Scope:** Ordering of SuperUnicode strings
**Applies To:** user interfaces, databases, search engines, sort comparators
**Last Updated:** 2026-09-30

---

## 1. Overview

Collation is the ordering contract for SuperUnicode strings. SUTR-2 records
the original report that introduced **SUCA**, the SuperUnicode Collation
Algorithm, and its design rationale.

> **Normative status.** This report is **superseded for normative purposes by
> [SUTS-001](../suts/SUTS-001-suca.md)**, which specifies the complete
> algorithm, weight scheme, data files, and conformance matrix. Where this
> report and SUTS-001 differ in detail, **SUTS-001 is authoritative**. SUTR-2
> is retained as the architectural record of *why* the algorithm was shaped the
> way it was.

### 1.1 The problem

Collation is not code-point order, and it is not a property of a string. It
is a property of the combination of the string, a **Collation Element Table**,
and a set of parametric settings. Collation varies by language — Germans,
French and Swedes sort the same characters differently — and by application
even within one language, since dictionaries, phonebooks and book indices
disagree.

### 1.2 SuperUnicode scope

SUCA orders the **full 64-bit** ExtSUCS domain, not just the 31-bit Base:

| Space | Range | Collation treatment |
|-------|-------|---------------------|
| Unicode Bridge | `0x00000000`–`0x0010FFFF` | Via its bridged Unicode repertoire; canonical-equivalence aware |
| SCP | `0x00110000`–`0x0011FFFF` | Variable: ignorable at L1–L3, resolving at L4 and the identical level |
| Native SUCS | `0x00120000`–`0x7FFFFFFE` | Algorithmic implicit weights from the codepoint |
| ExtSUCS plugins | above `0x7FFFFFFF` | Inherits Base ordering; extends above it |

Every SUTS and SUAS document in the family is ExtSUCS-compatible, and SUCA
carries the full 64-bit domain rather than truncating to 31 bits.

---

## 2. Multilevel Comparison

SUCA employs a **multilevel comparison algorithm**, as UTS #10 does. Accent
differences are normally ignored when base letters differ; case differences
are normally ignored when base letters or accents differ.

| Level | Description | Example ordering |
|-------|-------------|------------------|
| L1 | Base characters | `role` < `roles` < `rule` |
| L2 | Accents | `role` < `rôle` < `roles` |
| L3 | Case / variants | `role` < `Role` < `rôle` |
| L4 | Variable (punctuation) | `role` < `"role"` < `Role` |
| Identical | NFD codepoint tie-break | `role` < `role` + combining < `"role"` |

### 2.1 Canonical equivalence

Two sequences are **canonically equivalent** when they represent the same text
with different actual sequences. Canonically equivalent sequences MUST collate
equally. SUCA satisfies this by normalizing each input to **Normalization Form
D** before mapping — a precomposed character collates identically to its
decomposed form, and characters sort correctly across the whole bridge.

### 2.2 Contextual sensitivity

- **Contraction** — two or more characters collate as one base letter, e.g.
  Slovak `ch` after `h`.
- **Expansion** — one character collates as a sequence, e.g. `Œ` as `O`+`E`.
- **Backward accents** — in some French dictionary traditions the *last*
  accent difference decides, which UTS #10 models as a backward level.

### 2.3 Implicit weights

Codepoints with no explicit table entry receive algorithmic primary weights
derived from the codepoint itself, monotonic within their range. This is what
lets SUCA produce a determinant result for **every** codepoint in the 64-bit
domain — assigned, unassigned, reserved, SCP, and plugin alike — without a
table entry for each. Han ideographs in the bridge receive Siniform implicit
weights keyed off their bridge codepoint.

---

## 3. Design Decisions

The choices below are the substance of this report. Each is a decision that
could reasonably have gone the other way.

### 3.1 SCP controls are variable, not primary

The historical `SUCA.txt` contract was *binary order, control plane sorts after
printables*. SUCA reconciles that with multilevel collation by defining SCP
codepoints as **variable** collation elements: ignorable at L1–L3, resolving
only at L4 and the identical level. The identical level then restores binary
order for tie-breaking, so the old contract survives without a special case.
Native allocations, by contrast, get implicit **primary** weights and are *not*
variable — a native codepoint is content, not control.

### 3.2 The identical level restores binary order

A deliberate consequence: the identical level (SUTS-001 §7.4, S3.10) MUST
order by **SuperUnicode native codepoint order**, with the Sentinel treated as
highest. Binary tie-break therefore equals `SUCA.txt` binary ordering, and
existing binary-order assumptions in downstream code keep working.

### 3.3 The domain is 64-bit

SUCA was specified over `sucs_ex_char_t` from the outset rather than deferred
to a later extended version. Plugin ranges above `0x7FFFFFFF` sort after
Base, so the algorithm stays monotonic across the whole space and no
re-specification is needed when plugins mount.

### 3.4 No compression, no rendering

SUCA defines ordering only. It does not define search index structure, glyph
selection, or display; those belong to the storage and layout layers
(SUTR-3, SUAS-001, SUAS-002).

---

## 4. Variable Weighting

Variable collation elements — typically punctuation — follow the four UTS #10
options:

| Option | Behavior |
|--------|----------|
| **Non-ignorable** | Variable CEs are not reset; all mappings unchanged |
| **Blanked** | Variable CEs and subsequent ignorables reset to zero at all non-identical levels; no L4 |
| **Shifted** (default) | Variable CEs reset to zero at L1–L3 and gain an L4 weight; subsequent ignorables reset at L1–L4 |
| **Shift-trimmed** | As *shifted*, then trailing `FFFF`s are trimmed |

A single maximum variable primary `SUCA_VAR_MAX` separates variable from
ordinary primaries; all CEs with L1 ≤ `SUCA_VAR_MAX` are variable.

---

## 5. Data Files

| File | Domain |
|------|--------|
| `Public/0.1.0/collation/SUCA.txt` | Base SUCS contract; starts as the binary-order baseline, with a curated SUCET excerpt |
| `Public/0.1.0/collation/ExtUCA.txt` | ExtSUCS contract (64-bit); confirms Base sorts before plugin ranges and per-plugin tailoring is allowed |

The reference implementation (`suts_suca`, freestanding C99, no heap) embeds a
compiled subset of the SUCET, following the same pattern as the SUCD BiDi
subset in `suas_sucd`. The default table is **not** intended to give
linguistically correct sorting for every language without tailoring;
per-language tailoring is expected.

---

## 6. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| At least three collation levels | Yes | `suts_suca_*` provides L1–L4 plus identical |
| Canonical-equivalence equality | Yes | NFD normalization (SUTS-001 S1.1) |
| Contractions and expansions | Yes | CE-array construction (S2) |
| Sort key generation and binary compare | Yes | S3 / S4 |
| Variable weighting, four options | Yes | SUTS-001 §4 |
| Backward secondary (French) | Yes | S3.6–S3.8 |
| Implicit weights over 64-bit ExtSUCS | Yes | SUTS-001 §9.1 |
| Semi-stable / identical level | Yes | S3.10 |
| SCP controls variable, not primary | Yes | SUTS-001 §3, §4 |
| Identical level = native codepoint order | Yes | SUTS-001 §2.2, §7.4 |
| Determinant result for every 64-bit codepoint | Yes | Implicit weight derivation |
| ExtSUCS 64-bit codepoint API | Yes | `sucs_ex_char_t` inputs |
| Freestanding C99, zero heap | Yes | `-std=c99 -ffreestanding` |

---

## 7. Terms & Conventions

- **SUCA** — SuperUnicode Collation Algorithm.
- **Collation** — determining the sort order of strings of characters.
- **Collation Element (CE)** — an ordered list of collation weights.
- **Collation Element Table** — the set of CEs; the SUCET is the SuperUnicode
  default.
- **Collation Weight** — a non-negative integer establishing relative order.
- **Primary / Secondary / Tertiary / Quaternary** — weights 1–4.
- **Variable CE** — a primary CE with a low reserved primary weight, subject to
  variable weighting.
- **Sort key** — a binary-comparable array of weights.
- **Level separator** — a low integer separating weights of different levels.
- **Identical level** — the final tie-break, by native codepoint order.
- **Implicit weight** — an algorithmically derived weight for a codepoint with
  no table entry.
- **SUCS / ExtSUCS** — the 31-bit Base space and the unbounded 64-bit space.
- **SCP** — System Control Plane.
- **NFD** — Normalization Form D.
- **UTS #10** — Unicode Collation Algorithm; SUCA's multilevel model is
  modeled on it.

---

**END OF SUTR-2**
