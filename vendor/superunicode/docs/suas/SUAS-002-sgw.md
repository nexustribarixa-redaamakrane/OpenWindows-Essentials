# SUAS-002 — System Glyph Width & Monospace Grid (SGW)

**Standard ID:** SUAS-002
**Status:** Draft — Ratified
**Scope:** Core Kernel Invariant
**Applies To:** terminal emulators, CJK/ideographic framebuffer consoles, tabular renderers, streaming compositors
**Last Updated:** 2026-09-04

---

## 1. Overview

System Glyph Width & Monospace Grid (SGW) is the **core architecture**
governing single-byte / fixed-cell layout metrics in SuperUnicode. It defines
an East_Asian_Width-style width classification over the **full 64-bit SUCS
codepoint space** (Base SUCS + SCP + Native SUCS + ExtSUCS plugins), together
with an **O(1) monospace grid model** used by terminal emulators, framebuffer
consoles, and tabular/streaming compositors.

SGW is the structural successor to the Unicode East Asian Width property
(UAX #11). It retains the well-understood `F / H / W / Na / A / N` semantics
for the Unicode Bridge, then extends the model to the rest of SUCS and folds
it into a single *grid contract*: every decoded codepoint occupies **zero, one,
or two** fixed-cells, and a column cursor advances in **O(1)** time with no
allocation and no look-back into a shaped run.

### 1.1 Goals

- **Explicit fixed-cell metric.** Every codepoint resolves to a storage width
  (its intrinsic glyph advance) and a grid width (the monospace cell count it
  consumes); the two are distinct (§3).
- **O(1) grid calculation.** Grid cell count and column advance are computed
  in constant time from the codepoint, independent of run length.
- **Zero allocation.** All classification and grid math operate on immutable
  static data and caller-provided cursors; no heap memory is ever used.
- **Deterministic resolution.** Given the same codepoint and context, any
  conforming implementation MUST compute an identical width resolution.
- **Contextual resolution.** Ambiguous (`A`) codepoints resolve to narrow or
  wide based on the surrounding context, exactly as in UAX #11.

### 1.2 Non-Negotiable Invariants

1. **No heap.** The SGW engine performs no dynamic allocation; classification
   and grid math run entirely on immutable static tables and caller buffers.
2. **O(1) grid advance.** `suas_sgw_column_advance()` must run in constant
   time for a single codepoint, independent of the number of codepoints in a
   run.
3. **Zone dispatch.** Every codepoint is classified by zone: Unicode Bridge →
   curated East_Asian_Width table; SCP → non-advancing control; Native SUCS →
   scalable/virtual widths; ExtSUCS → Neutral by default (§5.4).
4. **Storage width ≠ grid width.** A real font's variable character advance is
   never confused with the fixed-cell grid metric (§3.1).
5. **Determinism.** Given the same codepoint and context, output is
   identical across conforming implementations.

---

## 2. Scope

SGW concerns the **horizontal advance** of codepoints in fixed-cell (monospace
and dual-width CJK) composition. It defines:

- The six width classes `F`, `H`, `W`, `Na`, `A`, `N` (analogous to UAX #11).
- The contextual resolution of Ambiguous codepoints.
- The mapping from width class to **grid cells** (0, 1, or 2) and to a
  **column cursor**.
- Zone policies for the non-Bridge SUCS districts.

SGW does **not** define glyph shapes, vertical metrics, line breaking, or
variable-font shaping. It is the layout-metric sibling of SUAS-001 (SDF) and
is compatible with the SUTS-001 (SUCA) collation space: both consume the same
64-bit `sucs_ex_char_t` codepoints.

> **Note on terminal tailoring.** As with UAX #11, the width property is
> *not* intended to be used without case-by-case tailoring by modern terminal
> emulators. SGW therefore exposes an explicit *tailoring hook* (§6.2) so a
> terminal can override the default resolution for specific codepoints or
> ranges without modifying the normative table.

---

## 3. Description

SGW models glyph advance at two distinct granularities.

### 3.1 Storage Width versus Grid Width

- **Storage width** is the intrinsic advance of a glyph in a *real, variable*
  font (e.g. a proportional Latin face or a composite emoji). It is a font and
  shaping concern, not a SGW concern.
- **Grid width** is the number of **fixed cells** a rendered codepoint *
  occupies* in a terminal or framebuffer console: `0` (non-advancing
  combining/control marks), `1` (halfwidth/narrow/neutral), or `2`
  (fullwidth/wide).

SGW only standardizes **grid width**. Storage width is left to the font
subsystem (SUF) and is out of scope. Conforming renderers MUST derive the
terminal cursor from grid width, never from storage width.

### 3.2 The Six Width Classes

| Class | Meaning | Grid cells (default) | Examples |
|-------|---------|----------------------|----------|
| `F` | **Fullwidth** | 2 | Unicode compatibility forms with `<wide>` decomposition |
| `H` | **Halfwidth** | 1 | Compatibility forms with `<narrow>` decomposition, `U+20A9 ₩ WON SIGN` |
| `W` | **Wide** | 2 | Han ideographs, Hangul, half-to-fullwidth CJK, Emoji_Presentation |
| `Na` | **Narrow** | 1 | ASCII, most Latin-1 (e.g. `U+00A5 ¥`) |
| `A` | **Ambiguous** | 1 or 2 by context | `U+01D4`, `U+212B Å` |
| `N` | **Neutral** | 1 | Neither wide nor narrow (most other codepoints; `U+01D3`, `U+00C5`) |

The class values are modeled on the Unic Econf East_Asian_Width property. The
attribute is **not closed under canonical equivalence** (§5.3): two canonically
equivalent codepoints MAY report different classes, so renderers and collators
must classify on the *accepted* (not decomposed) codepoint.

---

## 4. Definitions

- **ED1 — Width class.** One of `F`, `H`, `W`, `Na`, `A`, `N` (§5.1).
- **ED2 — Fullwidth (`F`).** A codepoint whose canonical or compatibility
  decomposition, when it exists, maps to a CJK ideograph or to a wide
  compatibility form.
- **ED3 — Halfwidth (`H`).** A codepoint with a `<narrow>` compatibility
  decomposition (e.g. halfwidth forms), or `U+20A9 ₩ WON SIGN`.
- **ED4 — Wide (`W`).** A codepoint that typically occupies two cells (Han
  ideographs, Hangul, Kana, CJK symbols, Emoji_Presentation). When the
  range is unassigned, an unassigned codepoint in a Han range is still `W`.
- **ED5 — Narrow (`Na`).** A codepoint that typically occupies one cell
  (ASCII and most non-CJK scripts).
- **ED6 — Ambiguous (`A`).** A codepoint that may be treated as either narrow
  or wide depending on context; defaults per §5.2.
- **ED7 — Neutral (`N`).** A codepoint that is neither wide nor narrow in any
  ordinary context; resolved to narrow by default.

- **Grid width.** The `0/1/2` cell metric (§3.1).
- **Storage width.** The intrinsic font advance; out of SGW scope (§3.1).
- **Column cursor.** The current horizontal raster position, in cells, over
  which `suas_sgw_column_advance()` advances by the grid width.

---

## 5. Classification

### 5.1 Default Width Assignment

Every codepoint in the 64-bit SUCS space is assigned exactly one width class.
The Unicode Bridge uses a **curated East_Asian_Width table** (a compact,
sorted range table embedded in the reference implementation, comparable to
the SUCD BiDi mask table). The rules that determine each assignment follow
UAX #11 and its companion UTS #51:

- **`F` (Fullwidth):** codepoints whose compatibility decomposition begins
  with, and is, a `<wide>` decomposition that maps to a CJK ideograph or wide
  form — e.g. the Fullwidth Forms block (`U+FF00`–`U+FF60`).
- **`H` (Halfwidth):** codepoints with a `<narrow>` compatibility
  decomposition such as the Halfwidth Forms block
  (`U+FF61`–`U+FFEF`), plus `U+20A9 ₩ WON SIGN`.
- **`W` (Wide):** CJK Unified Ideographs (`U+4E00`–`U+9FFF`,
  `U+3400`–`U+4DBF`, `U+F900`–`U+FAFF`, `U+20000`–`U+2FFFF`,
  `U+30000`–`U+3FFFF`), Hangul Syllables (`U+AC00`–`U+D7AF`), CJK
  Compatibility, Kana and Kana Extensions, Enclosed CJK, CJK Symbols and
  Punctuation, full range of CJK Compatibility Ideographs, and every
  character with the **Emoji_Presentation** property except those with the
  **Regional_Indicator** property (per UTS #51).
- **`Na` (Narrow):** ASCII, Latin-1 (including `U+00A5 ¥`), and the vast
  majority of non-CJK letters and digits which carry a narrow advance.
- **`A` (Ambiguous):** codepoints that may be narrow or wide, resolved per
  §5.2. Examples include `U+01D4`, `U+212B Å ANGSTROM SIGN`.
- **`N` (Neutral):** the default for most remaining codepoints, including
  `U+01D3`, `U+00C5 Å` (whose decomposition starts with a Narrow form), and
  the General Punctuation controls.

**Unassigned codepoints:** An unassigned codepoint in a CJK ideograph range
(CJK, CJK Extension A, CJK Compatibility, CJK Extension B/C/D/E/F/G) is `W`;
**all other** unassigned codepoints are `N`.

**Private use & replacement:** Private-use codepoints default to `A`, as does
the replacement character. Zone `SCP` (System Control Plane) defaults to a
non-advancing control width (§5.4).

### 5.2 Contextual Resolution of Ambiguous Codepoints

An `A` codepoint resolves narrowly by default. When the surrounding context
indicates a wide context (e.g. the resolved base direction or a governing
East_Asian_Width is Wide/Fullwidth), it resolves to wide. SGW exposes:

- `suas_sgw_resolve_ambiguous(cp, wide_context)` — returns fullwidth (2 cells)
  when `wide_context` is true, else halfwidth (1 cell).

Terminals that adopt a wide context for a run (such as a mixed ideographic
line) resolve all inner `A` codepoints to wide. This mirrors the UAX #11
contextual rules without requiring multi-pass look-back; the caller supplies
the context atom.

### 5.3 Canonical Equivalence Is Not Preserved

The width class is **not** preserved under canonical equivalence. Example:
`U+00C5 Å` is `N`, while its compatiblity counterpart `U+212B Å ANGSTROM
SIGN` is `A`. Classify the accepted codepoint directly; do not normalize
before classifying, or the grid metric will be wrong.

### 5.4 Zone Dispatch (64-bit SUCS)

| District | Policy |
|----------|--------|
| Unicode Bridge `0x00000000`–`0x0010FFFF` | Full East_Asian_Width table (`F/H/W/Na/A/N`) + UTS #51 Emoji_Presentation override. |
| SCP `0x00110000`–`0x0011FFFF` | **Control plane.** Non-advancing control codepoints: grid width `0`. |
| Native SUCS `0x00120000`–`0x7FFFFFFE` | **Scalable/virtual.** No ideographic scripts are defined here yet; default `1` cell (`N`). Reserved for future Han/Siniform native blocks, which will map to `W`. |
| ExtSUCS plugin `>0x7FFFFFFF` | **Neutral** single cell by default; a plugin may register a width override through the tailoring hook (§6.2). |
| Trap range `0x7FFFFFF0`–`0x7FFFFFFE` / sentinel `0x7FFFFFFF` | Not renderable; grid width `0`. |

---

## 6. Recommendations

### 6.1 Renderer Use

- Compute per-cell advance with `suas_sgw_cells()` and advance a column cursor
  with `suas_sgw_column_advance()`; both are **O(1)** and return the same
  value for a given codepoint + context.
- Never cursor-advance on combining marks (`0` cells): they overlay the
  preceding cell.
- For proportional fonts, use the font's own advance (SUF); SGW's grid is only
  authoritative for the fixed-cell console path.

### 6.2 Tailoring Hook

Modern terminals MUST be allowed to override default resolutions. SGW exposes
a per-instance override table (a caller-provided sorted range list consulted
**before** the normative table) so ranges of codepoints can be pinned to a
specific width class without mutating the immutable default table. When the
override list is empty (default), the normative table applies verbatim.

---

## 7. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| Six width classes `F/H/W/Na/A/N` | Yes | `suas_sgw_resolve()` returns each class. |
| O(1) grid cell + column advance | Yes | `suas_sgw_cells()` / `suas_sgw_column_advance()` are branch+binary-search-free constant-time per codepoint. |
| Zero heap allocation | Yes | No `malloc`/`realloc`/`free` in implementation. |
| Zone dispatch over 64-bit SUCS | Yes | `suas_sgw_resolve()` dispatches by §5.4. |
| Ambiguous contextual resolution | Yes | `suas_sgw_resolve_ambiguous()` per §5.2. |
| Emoji_Presentation → Wide (except Regional_Indicator) | Yes | Covered by curated table per UTS #51. |
| Unassigned Han ranges → Wide; others → Neutral | Yes | Curated table default policy per §5.1. |
| Canonical equivalence not preserved | Yes | Classify accepted codepoint directly (§5.3). |
| Tailoring hook | Yes | `suas_sgw_options_t` override list (§6.2). |
| Freestanding C99 | Yes | Compiles under `-std=c99 -ffreestanding`. |

---

## 8. Terms & Conventions

- **SCP** — System Control Plane (`0x00110000`–`0x0011FFFF`).
- **SUCD** — SuperUnicode Character Database.
- **SUCS** — SuperUnicode Character System; Base SUCS encodes 31-bit codepoints.
- **ExtSUCS** — the 64-bit `sucs_ex_char_t` codepoint space (Base + plugins).
- **EAW / UAX #11** — Unicode East Asian Width property; SGW's Unicode Bridge
  classification is modeled on it.
- **UTS #51** — Unicode Emoji; SGW consults `Emoji_Presentation` and
  `Regional_Indicator`.
- **Grid width** — the `0/1/2` fixed-cell metric (§3.1).
- **Storage width** — intrinsic font advance; out of SGW scope (§3.1).
- **Column cursor** — the horizontal raster position in cells (§4).

---

**END OF SUAS-002**
