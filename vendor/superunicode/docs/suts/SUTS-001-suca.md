# SUTS-001 — SuperUnicode Collation Algorithm (SUCA)

**Standard ID:** SUTS-001
**Status:** Draft — Authoring
**Scope:** Modular Technical Specification (string ordering / collation)
**Applies To:** Base SUCS (Unicode Bridge, SCP, Native), ExtSUCS (extended plane), databases, search engines, sort comparators
**Last Updated:** 2026-09-03

---

## 1. Introduction

Collation is the general term for the process and function of determining the
**sorting order of strings of characters**. It is a key function in computer
systems: whenever a list of strings is presented to users, they are likely to
want it in a sorted order so that they can easily and reliably find individual
strings. Thus it is widely used in user interfaces. It is also crucial for
databases, both in sorting records and in selecting sets of records with
fields within given bounds.

Collation varies according to language and culture: Germans, French and
Swedes sort the same characters differently. It may also vary by specific
application: even within the same language, dictionaries may sort differently
than phonebooks or book indices. For non-alphabetic scripts such as East
Asian ideographs, collation can be either phonetic or based on the appearance
of the character. Collation can also be customized according to user
preference, such as ignoring punctuation or not, putting uppercase before
lowercase (or vice versa), and so on. Linguistically correct searching needs
to use the same mechanisms.

It is important to ensure that collation meets user expectations as fully as
possible. **Collation is not code-point (binary) order**, and **collation is
not a property of strings** — it is a property of the combination of the
string, a **Collation Element Table**, and a set of **parametric settings**.

### 1.1 SuperUnicode Scope

SUCA is the SuperUnicode Collation Algorithm. As a **SuperUnicode Technical
Specification (SUTS)**, it specifies a complete, unambiguous, total ordering
for

1. **Base SUCS** — the 31-bit bounded space:
   * **Unicode Bridge** `0x00000000–0x0010FFFF` — collates via its bridged
     Unicode repertoire (canonical-equivalence aware);
   * **System Control Plane (SCP)** `0x00110000–0x0011FFFF` — system
     directives and formatting controls (collation-ignorable by default);
   * **Native SUCS** `0x00120000–0x7FFFFFFE` — native allocations with
     algorithmic (implicit) weights (sentinel `0x7FFFFFFF` last).
2. **ExtSUCS** — the unbounded 64-bit extended space, which inherits the
   entire Base SUCS ordering and extends it over plugin ranges above
   `0x7FFFFFFF`. **All SUTSs and SUASs of the SuperUnicode family are
   ExtSUCS-compatible**; SUCA carries its full 64-bit codepoint domain.

This document is the **first SUTS**. It elevates the former *SUTR-2 (SUCA)*
report to a normative technical specification and supplies a reference
implementation (`suts_suca`).

### 1.2 Multi-Level Comparison

SUCA, like UTS #10, employs a **multilevel comparison algorithm**. In
comparing two words, the most important feature is the identity of the base
letters; accent differences are typically ignored if the base letters differ;
case differences are typically ignored if base letters or accents differ;
punctuation handling varies. A final, tie-breaking **identical level** is used
when no other difference exists.

| Level | Description | Examples |
|-------|-------------|----------|
| L1 | Base characters | `role` < `roles` < `rule` |
| L2 | Accents | `role` < `rôle` < `roles` |
| L3 | Case / variants | `role` < `Role` < `rôle` |
| L4 | Punctuation | `role` < `"role"` < `Role` |
| Ln | Identical | `role` < `ro□le` < `"role"` |

### 1.3 Canonical Equivalence

Two sequences are **canonically equivalent** when they represent the same
text with different actual sequences. Canonically equivalent sequences MUST
collate the same. SUCA satisfies this by normalizing each input to
**Normalization Form D (NFD)** before weighting (S1.1), exactly as UTS #10
does, using SUCS-normalization data for the Unicode Bridge.

### 1.4 Contextual Sensitivity

* **Contraction** — two (or more) characters collate as a single base letter
  (e.g. Slovak `ch` after `h`).
* **Expansion** — a single character collates as a sequence (e.g. `Œ` as
  `O`+`E`).
* **Backward accents** — in some French dictionary traditions the *last*
  accent difference decides (S3.6–S3.8).

### 1.5 Merging Sort Keys, Determinism, Performance

* **Merged sort keys** — to sort by several fields while treating levels
  uniformly, callers may interleave the sort keys of fields (see
  §7.3.1). SUCA does not itself reorder records.
* **Deterministic comparison** — SUCA is, by default, *not* a deterministic
  comparison; the identical level (S3.10, §7.4) may be enabled for
  semi-stability.
* **Sort keys** — the canonical way to compare many strings fast: produce a
  binary-comparable key once and compare keys with a low-level memory
  operation. Incremental comparison (§8.4) is offered for one-off compares.

---

## 2. Conformance

A conformant implementation that purports to implement SUCA must do so as
described in this document. SUCA is a **logical specification**: an
implementation is free to change internals as long as any two strings are
ordered identically to the algorithm as specified.

### 2.1 Basic Conformance Requirements

* **SUCA-C1.** For a given SuperUnicode Collation Element Table (SUCET), a
  conformant implementation MUST replicate the same comparisons of strings as
  those produced by §7 (Main Algorithm). In particular, it MUST compare any
  two canonical-equivalent strings as equal, for all codepoints it supports.
* **SUCA-C2.** A conformant implementation MUST support at least three levels
  of collation. It may support four (or more).
* **SUCA-C3.** A conformant implementation that supports backward levels,
  variable weighting, or semi-stability MUST do so in accordance with this
  specification.
* **SUCA-C4.** An implementation that claims conformance MUST specify the
  SUCA version it conforms to.
* **SUCA-C5.** An implementation claiming conformance to searching and
  matching according to SUTS-001 MUST meet §10 (Searching and Matching).

### 2.2 Repertoire Conformance

* A conformant implementation MUST produce a determinant result for **every**
  codepoint in the 64-bit ExtSUCS domain, including unassigned, reserved,
  SCP, and plugin ranges, via implicit-weight derivation (§9.1).
* The identical level (§7.4, S3.10) MUST order by **SuperUnicode native
  codepoint order** (sentinel `0x7FFFFFFF` and `0xFFFFFFFF` treated as
  highest), so that binary tie-break equals `SUCA.txt` binary ordering.

---

## 3. Definitions and Notation

* **Collation Weight** — a non-negative integer establishing relative order
  of constructed sort keys. Shown hexadecimal, e.g. `0209`.
* **Collation Element (CE)** — an ordered list of collation weights:
  `[.l1.l2.l3.l4]`. A primary weight is the first; secondary, tertiary,
  quaternary follow.
* **Primary Weight** — the Level 1 (L1) weight. **Secondary** = L2,
  **Tertiary** = L3, **Quaternary** = L4.
* **Ignorable Weight** — a weight whose value is zero (`0000`).
* **Primary CE** — a CE whose L1 is non-zero. **Secondary CE** — L1 zero, L2
  non-zero. **Tertiary CE** — L1,L2 zero, L3 non-zero. **Quaternary CE** —
  L1..L3 zero, L4 non-zero. **Completely Ignorable CE** — all levels zero.
  **Ignorable CE** — not a primary CE.
* **Variable Collation Element** — a primary CE with a low (non-zero)
  primary weight reserved for punctuation/symbols; subject to §4 Variable
  Weighting. Marked with `*` in table listings.
* **Simple Mapping** — one codepoint → one CE.
* **Expansion** — one codepoint → a sequence of >1 CEs.
* **Contraction** — a sequence of >1 codepoints → one (many-to-one) or more
  (many-to-many) CEs.
* **Sort Key** — an array of non-negative integers built by extracting
  weights from a CE array (§7.3), suitable for binary comparison.
* **Level Separator** — a low integer (zero by default) separating weights
  from different levels in a sort key.
* **Backward at a Level** — a setting causing that level's weights to be
  scanned in reverse order (S3.6).
* **Non-Starter** — an assigned character with Canonical_Combining_Class ≠ 0.
  **Unblocked Non-Starter** — a non-starter not in a blocking context.
* **ExtSUCS codepoint** (`sucs_ex_char_t`) — a 64-bit ExtSUCS codepoint
  address; every SUCA input codepoint is expressed in this domain.

### 3.1 Comparison Notation

`X =n Y`, `X <n Y` follow UTS #10: a level-specific equality/inequality
between collation elements. `A ≡ B` means equivalent at all levels;
`A = B` means bit-for-bit identical.

---

## 4. Variable Weighting

Variable collation elements (typically punctuation) require special
handling. SUCA supports the four UTS #10 options:

| Option | Behavior |
|--------|----------|
| **Non-ignorable** | Variable CEs are not reset; all mappings unchanged. |
| **Blanked** | Variable CEs and subsequent ignorables are reset so all weights (except identical) are zero; no L4. |
| **Shifted** (default) | Variable CEs reset to zero at L1–L3 and gain an L4 weight; subsequent ignorables reset at L1–L4. |
| **Shift-Trimmed** | Same as Shifted, then trailing `FFFF`s are trimmed. |

**SuperUnicode nuance:** SCP control codepoints are defined as variable
collation elements (controls are collation-ignorable at L1–L3, resolving only
at L4/identical), while Native SUCS allocations are given implicit primary
weights and are *not* variable. This reconciles the historical
`SUCA.txt` "binary order, control plane sorts after printables" contract
with multilevel collation: the identical level restores that binary order for
tie-breaking.

### 4.1 Interleaving

Primary weights of variable CEs are not interleaved with non-variable
primaries; there is a single **maximum variable primary** `SUCA_VAR_MAX`.
All CEs with L1 ≤ `SUCA_VAR_MAX` are variable; all others are not.

---

## 5. Well-Formedness of SUCET

A well-formed SuperUnicode Collation Element Table MUST:

* consist of collation weights (non-negative integers);
* give each CE the same number of levels;
* contain no ambiguous mappings;
* satisfy WF1–WF5 of UTS #10 (no zero weight above a non-zero at a lower
  level; secondary/tertiary minimum conventions; no variable with ignorable
  primary; no interleaving; prefix contractions for non-starter-ending
  contractions).

---

## 6. Default SuperUnicode Collation Element Table (SUCET)

The default table is provided as a **programmatic embedded subset** in the
reference implementation plus an accompanying data description
(`collation/SUCA.txt`, `collation/ExtUCA.txt`). It provides a reasonable
default ordering for the documented repertoire:

1. **Unicode Bridge** — a curated subset (ASCII, Latin-1 Supplement,
   Latin Extended-A, digits, punctuation) with DUCET-style weight semantics,
   plus **implicit weights** for the Han ideographs and all other bridged
   codepoints.
2. **SCP** — variable (ignorable at L1–L3).
3. **Native SUCS + ExtSUCS plugin ranges** — algorithmic implicit primary
   weights derived from the 64-bit codepoint (see §9.1).

The table is **not** intended to provide linguistically correct sorting for
every language without tailoring; per-language tailoring is expected.

---

## 7. Main Algorithm

The main algorithm has four steps, exactly as UTS #10:

1. **Normalize** each input string (S1.1, NFD).
2. **Produce an array of collation elements** for each string (S2.1–S2.5).
3. **Produce a sort key** for each string (S3.1–S3.10).
4. **Compare** the two sort keys with a binary comparison (S4).

### 7.1 Normalize Each String (S1.1)

Convert the string into Normalization Form D. Implementations may skip this
step if they produce identical results.

### 7.2 Produce Collation Element Arrays (S2)

Walk the normalized string, at each point:

* **S2.1** Find the longest initial substring with a match in the SUCET.
  * **S2.1.1** For each following non-starter `C` (an *unblocked* non-starter),
    **S2.1.2** test whether `S+C` matches, **S2.1.3** and if so extend `S`.
* **S2.2** Fetch the corresponding CE(s), or **synthesize** an implicit CE
  (§9.1) if there is no match.
* **S2.3** Process the CE(s) per the variable-weight setting (§4).
* **S2.4** Append to the CE array.
* **S2.5** Advance past `S`; repeat.

### 7.3 Form Sort Keys (S3)

* **S3.1** For each weight level L from 1 to the maximum:
* **S3.2** If L not 1, append a level separator (zero).
* **S3.3–S3.5** If forward: append each non-zero `CE[L]`.
* **S3.6–S3.9** If backward: list non-zero `CE[L]`, reverse, append.
* **S3.10** If semi-stable (identical level): append a copy of the NFD string.

#### 7.3.1 Merged Sort Keys

Callers may merge sort keys field-by-field at each level (L1 of all fields,
then L2 of all fields, …) to sort database records by several fields with
uniform level handling.

### 7.4 Compare Sort Keys (S4)

Binary comparison: L3 differences are ignored if any L1/L2 difference exists;
L2 differences ignored if any L1 difference; L1 never ignored. The identical
level (when enabled) further distinguishes by **SuperUnicode native codepoint
order** (sentinel highest — see §2.2).

---

## 8. Support for Tailoring and Parametric Settings

### 8.1 Parametric Settings

| Attribute | Default | Values |
|-----------|---------|--------|
| Strength | tertiary | primary, secondary, tertiary, quaternary, identical |
| Variable weighting | shifted | non-ignorable, blanked, shifted, shift-trimmed |
| Backward levels | none | secondary (French) |
| Case ordering | lower-first via L3 | upper-first, lower-first |
| Normalization | on (NFD) | on, off |
| Semi-stable (identical) | off | on, off |

### 8.2 Tailoring Syntax

The reference implementation accepts a programmatic tailoring: rule lists of
the form

```
& base  <  x        # x primary-greater than base
& base  << x        # x secondary-greater than base
& base  <<< x       # x tertiary-greater than base
& base  =  x        # x equal to base
```

either side possibly a multi-character sequence (contraction/expansion).

### 8.3 Use of Combining Grapheme Joiner (CGJ)

`U+034F` (bridge) blocks contractions and inverts secondary ordering, exactly
as UTS #10 §8.3, without further tailoring.

### 8.4 Incremental Comparison

For one-off compares, implementations should generate CEs incrementally and
stop at the first difference (equivalent to full sort-key comparison).

---

## 9. Weight Derivation

### 9.1 Implicit Weights

Codepoints without an explicit mapping get implicit primary weights
(continuation-pair form, per UTS #10 §10.1.3), computed over the **full
64-bit ExtSUCS domain**. The scheme partitions the space so that:

* the **Unicode Bridge** implicit range sorts before **SCP**, which sorts
  before **Native/ExtSUCS** implicit ranges only at the *identical* level
  (see §2.2); at primary level, SCP is variable (ignored);
* within each range, codepoints sort in native codepoint order.

Han ideographs in the bridge (CJK Unified/Ideographs extensions A–J and
Compatibility Ideographs) receive **Siniform implicit weights** analogous to
UTS #10, keyed off their bridge codepoint.

### 9.2 Tertiary Weight Table (case/variant semantics)

Curated subset follows the DUCET conventions: `MIN3 = 0002` (lowercase /
unmarked), uppercase `0008`, compatibility variants `0004`, `0005`, etc.
See the reference table in `src/suts/suts_suca.c`.

---

## 10. Searching and Matching

Matching is collation at a chosen strength: strings that compare equal at
that strength match. SUCA supports:

* **minimal / medial / maximal** matches and boundary conditions
  (`whole-character`, `whole-word`), following UTS #10 §11;
* **collation folding** (§10, DS3) for fast mapping of a match family to a
  canonical folded string;
* **asymmetric search** (query-unmarked matches target-marked) §10.2.

---

## 11. Data Files

* `Public/0.1.0/collation/SUCA.txt` — Base SUCS collation contract (starts
  as the binary-order baseline; a curated SUCET excerpt is provided).
* `Public/0.1.0/collation/ExtUCA.txt` — ExtSUCS contract (64-bit); confirms
  Base sorts before plugin ranges and per-plugin tailoring is allowed.

The reference implementation embeds a compiled subset of the SUCET
(`suts_suca_*`), following the same pattern as the SUCD BiDi subset in
`suas_sucd`.

---

## 12. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| At least 3 collation levels | Yes | `suts_suca_*` provides L1–L4 + identical. |
| Canonical-equivalence equality | Yes | NFD normalization (S1.1). |
| Contractions & expansions | Yes | CE-array construction (S2). |
| Sort key generation + binary compare | Yes | S3/S4. |
| Variable weighting (4 options) | Yes | §4. |
| Backward secondary (French) | Yes | S3.6–S3.8. |
| Implicit weights over 64-bit ExtSUCS | Yes | §9.1. |
| Semi-stable / identical level | Yes | S3.10. |
| Freestanding C99, zero heap | Yes | `-std=c99 -ffreestanding`, no heap. |
| ExtSUCS 64-bit codepoint API | Yes | `sucs_ex_char_t` inputs. |

---

## 13. Terms & Conventions

* **SUTS** — SuperUnicode Technical Specification (modular driver/extension
  specs). SUTS-001 is SUCA.
* **SUCS** — SuperUnicode Character System (31-bit Base: bridge/SCP/native).
* **ExtSUCS** — extended SuperUnicode (unbounded, 64-bit in impl), inherits
  Base SUCS and is compatible with all SUTSs/SUASs.
* **SCP** — System Control Plane (`0x00110000–0x0011FFFF`).
* **SUCET** — SuperUnicode Collation Element Table (default and tailored).
* **SUCD** — SuperUnicode Character Database.
* **CE** — Collation Element. **NFD** — Normalization Form D.
* **CGJ** — Combining Grapheme Joiner.

---

**END OF SUTS-001**
