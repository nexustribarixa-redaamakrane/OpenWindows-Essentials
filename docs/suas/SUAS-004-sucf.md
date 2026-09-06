# SUAS-004 — SuperUnicode Canonical Forms (SUCF)

**Standard ID:** SUAS-004
**Status:** Draft — Ratified
**Scope:** Core Kernel Invariant
**Applies To:** filesystem drivers, identifier/key comparison, hashing, storage serializers, sorting pipelines, text search/indexing, diacritic-stripping analysis
**Last Updated:** 2026-09-05

---

## 1. Overview

SuperUnicode Canonical Forms (SUCF) is the **core architecture governing
canonical equivalence** in SuperUnicode. It defines two canonical transforms —
**SUCF-C** (Canonical Composition) and **SUCF-D** (Canonical Decomposition) —
over the **full 64-bit SUCS codepoint space** (Base SUCS + SCP + Native SUCS +
ExtSUCS plugins), through a **single-pass, zero-allocation** reordering and
(de)composition engine.

SUCF is the structural successor to the Unicode Normalization Forms (UAX #15,
`UAX15.txt` / `UnicodeData.txt` + `CompositionExclusions.txt`). It retains the
well-understood canonical-equivalence semantics of NFC/NFD for the Unicode
Bridge, then extends the model to the rest of SUCS and folds it into a single
**dual-target canonical contract**:

- **SUCF-C** — Canonical Composition, the standard in-memory/binary storage
  format: compact, fully composed, fast equality.
- **SUCF-D** — Canonical Decomposition, the exploded format for analysis,
  diacritic stripping, and sorting pipelines.

### 1.1 Goals

- **Single-pass canonical transform.** Both SUCF-C and SUCF-D fall out of a
  single streaming pass over the codepoints, without multi-pass buffer
  preprocessing.
- **Zero allocation.** Reordering and (de)composition happen inside a small
  **stack-allocated sliding window** (O(1) dynamic heap footprint). All tables
  are immutable static data; all working state is caller-provided.
- **Deterministic canonical ordering.** Within a combining-character sequence,
  marks are sorted *ascending* by Combining Canonical Class (CCC), with an
  O(1) swap rule when two adjacent marks are out of order. The same input
  always produces the same canonical sequence.
- **SCP immunity.** SCP instructions, BANcodes, and trap markers are
  **canonically invariant** — they pass through the engine untouched and never
  disturb the combining-reordering state.
- **Unicode Bridge compatibility.** For `0x000000`–`0x0010FFFF`, SUCF
  reproduces Unicode canonical equivalence exactly, via binary property tables
  in SUCD. A normalized SuperUnicode string normalizes the same way as its
  Unicode equivalent.

### 1.2 Non-Negotiable Invariants

1. **No heap.** The SUCF engine performs no dynamic allocation; tables are
   immutable static data and all working state is caller-provided.
2. **O(1) dynamic state.** The reordering window is a fixed, stack-allocated
   buffer; memory does not grow with run length.
3. **Two canonical forms.** SUCF-C (composition) and SUCF-D (decomposition)
   are the only canonical targets; both derive from the same decomposition
   data.
4. **CCC ascending reordering.** Non-zero combining marks sort ascending by
   CCC; starters (CCC 0) are never moved by reordering.
5. **SCP immunity.** Control-plane codepoints are invariant and preserve the
   engine state.
6. **Determinism.** Given the same codepoints, any conforming implementation
   MUST produce an identical SUCF-C and SUCF-D output.

---

## 2. Scope

SUCF concerns **canonical equivalence** over the 64-bit space. It defines:

- The **two canonical targets** (SUCF-C composition, SUCF-D decomposition).
- The **Combining Canonical Class (CCC)** and its **ascending reorder
  contract**.
- The **decomposition table** over the Unicode Bridge (via SUCD binary
  properties) and the **Hangul algorithmic** decomposition/composition.
- **SCP immunity** for control-plane, BANcode, and trap codepoints.
- **Zone policy** for the non-Bridge SUCS districts.
- **Quick check** and the **stability / concatenation** guarantees.

SUCF does **not** define compatibility decomposition (the ≪ compatibility
subset, e.g. `U+FB01` ﬁ → `U+0066 U+0069`): compatibility folding is a
*separate, optional* caller concern and is out of scope for the canonical
forms. SUCF is the canonical-equivalence sibling of SUCA (collation) and is
consumed by SBR (line breaking), SGW (width/grid), and the filesystem /
identifier-key layers.

---

## 3. Description

### 3.1 Canonical Equivalence and the Two Targets

Two strings are **canonically equivalent** if they normalize identically under
both SUCF-C and SUCF-D. Canonical equivalence preserves *visual* identity
(e.g. `U+00E9` é ≡ `U+0065 U+0301` e + combining acute) and is the basis for
identifier and key comparison.

- **SUCF-C** is the **storage/equality** target: maximally composed, producing
  the smallest canonical representation so equality can be checked by fast byte
  or word comparison. It is the recommended default for in-memory strings and
  on-disk identifiers.
- **SUCF-D** is the **analysis** target: fully decomposed (exploded), suitable
  for diacritic stripping (removing zero-width and combining marks for search
  indexing) and for sort pipelines that need the decomposed skeleton.

### 3.2 The Combining Canonical Class (CCC) and Reordering

Every codepoint that combines carries a **Combining Canonical Class (CCC)**, a
value in `0..254` (as Unicode). A codepoint with CCC 0 is a **starter** (a base
glyph or a non-combining character); a codepoint with a non-zero CCC is a
**combining mark** that attaches to the preceding starter.

Canonical ordering is deterministic per the **CCC ascending reorder
contract**:

> Within any run of codepoints following a starter, defer marks with non-zero
> CCC, then reorder them **ascending by CCC**. Starters (CCC 0) are never
> reordered past one another, and a starter always precedes its marks.

At the reordering engine's heart is the **O(1) swap rule**:

> If two *adjacent* marks have CCC_A > CCC_B and CCC_B ≠ 0, they must be
> swapped (reordered ascending). They may be exchanged only otherwise; two
> adjacent marks with equal (non-zero) CCC keep their relative order (stable).

The sliding-window engine holds at most a fixed number of pending non-zero-CCC
marks after the current starter; when the window fills (the stream-safe bound,
see §3.4), it flushes the earliest pending mark to the output. This bounds
state to O(1) while enforcing the ascending order across arbitrary runs.

### 3.3 The Dual-Target Transform

Both targets are computed in a single pass by the same engine, parameterized
by the target form:

1. **Decompose** each incoming codepoint into its canonical decomposition
   (algorithmically for Hangul, via the SUCD binary table for the Bridge).
2. **Reorder** the decomposed marks by CCC ascending (§3.2).
3. **Compose** (SUCF-C only): after reordering, attach the first combining
   mark to the preceding starter if the *composition* exists (i.e. the pair
   `starter + mark` is in the composition table and neither participant is
   excluded from composition). SUCF-D stops after step 2.

Because the engine is streaming and single-pass, SUCF-C and SUCF-D run in
linear time with O(1) dynamic memory, in a single forward traversal.

### 3.4 Stream-Safe Format

To guarantee the O(1) window bound even for pathological input, SUCF adopts
the **stream-safe format** convention: the reordering engine is not obliged to
reorder across more than **30 non-starter codepoints** following a starter
(the UAX #15 convention), matching a **buffer of 32** codepoints in the
sliding window. When more than 30 non-starters follow a starter, the engine
flushes the earliest ones so that no run reorders across the stream-safe
boundary. A conforming implementation visible in user data is permitted to
insert the **Graphical Join Character** (`CGJ`, `U+034F`) at any stream-safe
position without changing the canonical representation, and the reorder
engine treats CGJ as a zero-width, non-reordering separator.

---

## 4. Definitions

- **ED1 — Starter.** A codepoint with CCC 0 that begins a combining sequence.
- **ED2 — Combining mark.** A codepoint with non-zero CCC, attached to the
  preceding starter.
- **ED3 — CCC (Combining Canonical Class).** A value in `0..254` that
  determines mark ordering (§3.2).
- **ED4 — Canonical equivalence.** Two strings normalize identically under
  SUCF-C and SUCF-D (§3.1).
- **ED5 — Canonical decomposition.** The fully decomposed (exploded) form of a
  codepoint string (SUCF-D).
- **ED6 — Canonical composition.** The maximally composed form of a codepoint
  string (SUCF-C).
- **ED7 — Quick check.** A per-codepoint hint (`YES`, `NO`, `MAYBE`) that some
  strings are already in a given form and can skip a full transform.
- **ED8 — Stable codepoint.** A codepoint that is identical in all four
  canonical senses in every version; stable codepoints are never normalized
  away or changed (§7).
- **ED9 — SCP-immunity.** Control-plane codepoints pass through unchanged
  without disturbing state (§5.4).

---

## 5. Classification & Invariance

### 5.1 Zone Dispatch

| District | Policy |
|----------|--------|
| Unicode Bridge `0x00000000`–`0x0010FFFF` | Full UAX #15 canonical-equivalence model: CCC + canonical decomposition + composition from the SUCD binary property tables; Hangul invariant (§5.6). |
| SCP `0x00110000`–`0x0011FFFF` | **SCP immunity.** Machine instructions, BANcodes, and control directives pass through **unchanged** and do **not** break the combining-reorder state (§5.4). |
| Native SUCS `0x00120000`–`0x7FFFFFFE` | **Canonically invariant.** Native codepoints are starters (CCC 0) by default; they neither combine nor reorder against Bridge marks unless a native allocation defines otherwise. |
| ExtSUCS plugin `>0x7FFFFFFF` | **Canonically invariant** default; a plugin may register CCC/decomposition properties via the tailoring hook (§6.3). |
| Trap range `0x7FFFFFF0`–`0x7FFFFFFE` / sentinel `0x7FFFFFFF` | **Invariant.** Trap markers and the end-of-stream sentinel pass through untouched and terminate any open combining state. |

### 5.2 The Combining Canonical Class (CCC)

For the Unicode Bridge, CCC is taken from the SUCD binary property table
(`Canonical_Combining_Class`), a curated, sorted range table defaulting to
CCC 0. The reference implementation exposes the curated values in
`suas_sucf.h` (the `suas_sucf_ccc()` lookup). All other SUCS districts default
to CCC 0 (starters) unless a plugin registers otherwise.

Reordering applies only when both adjacent marks are non-zero CCC: a swap
happens iff `CCC_A > CCC_B` and `CCC_B ≠ 0`. Equal-C or zero-CCC marks never
swap (the order is stable).

### 5.3 Canonical Decomposition & Composition Data

The Unicode Bridge decomposition/composition data is a **sorted** table of
canonical mappings (from SUCD `UnicodeData.txt` + `CompositionExclusions.txt`
modeled as binary properties). The reference implementation curates a
representative subset covering all composition exclusions and the common
Latin/Greek/Cyrillic precomposed forms, with SUCD as the source of truth.

**Composition exclusion types** are honored:
- **Script-specific exclusions** — canonical decompositions that the standard
  refuses to compose (e.g. some scripts where composition is prohibited),
- **post-composition-version exclusions** — codepoints that did not compose at
  the time they were canonically added and therefore stay decomposed,
- **singleton exclusions** — a starter decomposing to a single codepoint (e.g.
  `U+212B Å → U+00C5 Å`),
- **non-starter decompositions** — a decomposition whose first element is
  itself a non-starter; the pair never composes.

A codepoint excluded from composition is passed through decomposed even in
SUCF-C.

### 5.4 SCP Immunity and Control-Plane Invariance

**SCP immunity** is a non-negotiable invariant: codepoints in
`0x00110000`–`0x0011FFFF` (the System Control Plane — machine instructions,
BANcode registry blocks, trap dispatch) are **canonically invariant**. They
pass through the engine byte-identical, and their presence does **not** reset
or corrupt the combining-reorder window: a combining sequence started before
an SCP codepoint resumes reordering after it, and an SCP codepoint is never
treated as a starter that would detach pending marks. This guarantees that
control-marked streams normalize identically whether or not the control data
is present.

### 5.5 Quick Check

SUCF exposes a **quick check** hint per form: `YES`, `NO`, or `MAYBE`. A
string whose codepoints are all `YES` for SUCF-C is already in canonical
composition and can skip the transform; a `NO` requires transformation; a
`MAYBE` requires the full routine to be certain. Quick check is a performance
fast-path and never changes the result.

### 5.6 Hangul

Hangul syllables are handled **algorithmically** (no table), exactly as
Unicode. A precomposed syllable `S = 0xAC00 + L × 21 × 28 + V × 28 + T`
decomposes algorithmically into its Jamo (L, V, and optionally T); conversely
an `L V` / `L V T` Jamo run composes algorithmically back into a
syllable. Hangul Jamo are CCC 0 starters with respect to the reorder engine.

---

## 6. Recommendations

### 6.1 Storage / Identifier Use

- Prefer **SUCF-C** for identifiers, filesystem names, and any on-disk or
  in-memory canonical key: it is compact and equality can be checked by
  direct comparison.
- Use **SUCF-D** for analysis pipelines that need the decomposed skeleton
  (diacritic stripping for search) and for sorting.
- Always normalize both sides of a comparison to the **same** target before
  comparing; SUCF-C and SUCF-D are not interchangeable as equality bases
  without a full transform.

### 6.2 Stream Handling

- Feed the engine codepoints in forward order with
  `suas_sucf_process_codepoint()` and flush at end of stream
  (`suas_sucf_flush()`), or use the bulk `suas_sucf_transform()` for a whole
  array.
- Treat the trap sentinel as an end of text, flushing any pending marks.
- For multi-part concatenation, carry the `suas_sucf_state_t` across segment
  boundaries (see §7 concatenation warning).

### 6.3 Tailoring Hook and Plugin Properties

As with SGW/SBR, SUCF exposes a caller-provided override table consulted
**before** the normative tables. A protocol or plugin may pin ranges of
codepoints to a specific CCC or (non-)exclusion status without mutating the
immutable default table. When the list is empty (default), the normative
model applies.

### 6.4 Diacritic Stripping (Analysis Aid)

To strip diacritics for search, run SUCF-D, then drop codepoints whose CCC is
non-zero and whose *canonical* mapping is to a base letter plus a mark. This
is an application responsibility layered on SUCF-D, not a separate form.

---

## 7. Stability, Versioning, and Concatenation

- **Backward compatibility.** SUCF is **versioned**: the decomposition and
  composition data are fixed to the SUCD version used by a given SuperUnicode
  release. New characters may be added in later versions, but existing
  canonical mappings never change, so data normalized under an older version
  remains canonical under the newer one (as in Unicode).
- **Composition version.** A codepoint is excluded from composition if it was
  excluded at the time it was canonically added (post-composition version);
  the exclusion is permanent for the class of that codepoint.
- **Stable codepoints.** Certain codepoints are **stable**: they have the same
  (identity) decomposition and are neither composed into nor decomposed out of
  in any version.
- **Concatenation is not guaranteed closed.** The concatenation of two
  canonically-normalized strings is **not** necessarily canonical: a mark at
  the start of the second segment may compose with, or reorder against, the
  tail of the first. Callers MUST re-normalize after concatenation, or carry
  the streaming state across the join.

---

## 8. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| Two canonical targets (SUCF-C composition, SUCF-D decomposition) | Yes | `suas_sucf_transform` with `SUAS_SUCF_FORM_C`/`FORM_D`. |
| Single-pass canonical transform | Yes | One forward pass, no multi-pass buffer. |
| Zero heap allocation / O(1) sliding window | Yes | Fixed stack-allocated window; no `malloc`/`realloc`/`free`. |
| CCC ascending reorder (swap iff CCC_A > CCC_B, CCC_B ≠ 0) | Yes | Reorder engine + tests. |
| SCP immunity (control plane invariant) | Yes | SCP/BANcode/trap pass-through + state preservation tests. |
| Unicode Bridge canonical-equivalence compat | Yes | SUCD binary decomposition/composition tables; Hangul algorithmic. |
| Quick check (YES/NO/MAYBE) | Yes | `suas_sucf_quick_check`. |
| Stream-safe format / combining-window bound | Yes | Window flush at the 30-non-starter bound. |
| Hangul algorithmic (de)composition | Yes | No table entries for Hangul syllable block. |
| Tailoring hook | Yes | `suas_sucf_options_t` override list (§6.3). |
| Freestanding C99 | Yes | Compiles under `-std=c99 -ffreestanding`. |

---

## 9. Terms & Conventions

- **SUCF** — SuperUnicode Canonical Forms; the SUAS-004 canonical
  (de)composition architecture.
- **SUCF-C** — Canonical Composition; the storage/equality target (§3.1).
- **SUCF-D** — Canonical Decomposition; the analysis/sorting target (§3.1).
- **CCC** — Combining Canonical Class (`0..254`); the mark-ordering value.
- **SCP** — System Control Plane (`0x00110000`–`0x0011FFFF`); canonically
  invariant.
- **SUCD** — SuperUnicode Character Database; source of Bridge decomposition/
  composition data.
- **SUCS** — SuperUnicode Character System; Base SUCS encodes 31-bit codepoints.
- **ExtSUCS** — the 64-bit `sucs_ex_char_t` codepoint space (Base + plugins).
- **UAX #15** — Unicode Normalization Forms; SUCF's Unicode Bridge semantics
  are modeled on it.
- **CGJ** — `U+034F` COMBINING GRAPHEME JOINER; a zero-width, non-reordering
  separator (§3.4).
- **Starter** — a CCC-0 codepoint that begins a combining sequence.
- **Combining mark** — a non-zero-CCC codepoint attached to a starter.

---

**END OF SUAS-004**
