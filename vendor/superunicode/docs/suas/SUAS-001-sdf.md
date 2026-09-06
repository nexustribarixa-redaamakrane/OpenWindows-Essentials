# SUAS-001 — Structural Directional Framing (SDF)

**Standard ID:** SUAS-001
**Status:** Draft — Ratified
**Scope:** Core Kernel Invariant
**Applies To:** System Control Plane (SCP), SUTF / SUST decoding streams, renderers
**Last Updated:** 2026-09-02

---

## 1. Overview

Structural Directional Framing (SDF) is the **core architecture** governing
bi-directional text layout, scope isolation, and glyph mirroring in
SuperUnicode. It is a **non-negotiable** invariant: every conforming
implementation MUST resolve bidirectional text through the SDF engine, and
MUST NOT fall back to any other multi-pass algorithmic model.

SDF is the structural successor to the Unicode Bidirectional Algorithm
(UAX #9). It replaces the multi-pass, buffer-reordering runtime model with a
**single-pass, zero-allocation, state-machine-driven pipeline** that is
carried within the System Control Plane (SCP) and the SUTF/SUST decoding
streams.

### 1.1 Goals

- **Explicit directionality.** Directional shifts and isolates are declared
  in-band via dedicated SCP codepoints — never inferred heuristically and
  never reordered in user memory.
- **Zero dynamic allocation.** Isolation scope is tracked on an internal
  fixed-depth stack. All operations are `O(1)`; no heap memory is ever used.
- **Renderer-native mirroring.** Framed output words carry an auto-mirror
  bit and a directional classification, letting framebuffer compositors
  mirror paired glyphs instantly with no post-processing pass.
- **Single-pass determinism.** A codepoint stream is consumed exactly once;
  its visual framing is determined at decode time.

### 1.2 Non-Negotiable Invariants

1. **No buffer reordering.** SDF MUST NOT reorder string buffers in user
   memory. Logical order is preserved; visual order is a renderer
   interpretation of framing metadata.
2. **No heap.** The SDF engine allocates exclusively from a fixed
   compile-time stack embedded in the framing context.
3. **Dual-mode direction resolution.** Directions are resolved by the
   **Dual-Mode Directional Resolver** (§5.3). Scope *changes* (push/pop/switch)
   happen only via the four SCP directives defined in §3; within the Unicode
   Bridge, per-codepoint direction is resolved *implicitly* from SUCD BiDi
   properties (§5.4). For renderers that require full lexical bidirectional
   layout, SDF additionally implements the **full Unicode Bidirectional
   Algorithm processing model** (UAX #9 semantics, §5.5): paragraph levels
   (P1–P3), explicit embedding/override/isolate levels (X1–X8), weak and
   neutral resolution (W1–W7, N0–N2), implicit levels (I1–I2), and reordering
   with mirroring (L1–L4). The framer remains single-pass and deterministic.
4. **Determinism.** Given the same valid codepoint stream, any conforming
   implementation MUST compute an identical framing sequence.

---

## 2. Scope Isolation Model

Directionally active content is organized as a **stack of isolates**.
Each isolate defines its own directional context. At any point in a stream
exactly one isolate is **active** (the top of the stack).

The stack is implemented as a fixed-capacity array inside the framing
context (see `suas_sdf_state_t`). It has a compile-time depth `SUAS_SDF_STACK_DEPTH`
(default **32**). Pushing onto a full stack or popping an empty stack is a
**structural error** reported via the framer status — the engine never
overflows or underflows memory.

### 2.1 Isolate Frame

Each stack entry records:

| Field | Description |
|-------|-------------|
| `base_dir` | The explicit base direction installed when the isolate opened. |
| `cur_dir`  | The live/resolved direction at the top of the active isolate. |

Levels are **not** tracked as parity-derived depths (as in UAX #9); they are
explicitly opened and closed by paired SCP directives.

---

## 3. SCP Directional Directives

SDF defines four directional control codepoints. They reside in the SCP
(**District `0x11`**) in the dedicated **Directional Control block** at
`0x00110100`–`0x0011010F`, which is free of collisions with the existing
formatting block (`0x00110000`–`0x001100FF`) and the BANcode Registry
(`0x0011A000`–`0x0011AEFF`).

| Codepoint | Directive | Semantics |
|-----------|-----------|-----------|
| `0x00110101` | **`SCP_DIR_LTR`** | Set the resolved direction of the active isolate to left-to-right. |
| `0x00110102` | **`SCP_DIR_RTL`** | Set the resolved direction of the active isolate to right-to-left. |
| `0x00110104` | **`SCP_DIR_ISOLATE_PUSH`** | Open a new isolate; its `base_dir` is taken from the current resolved direction, and it becomes active. |
| `0x00110108` | **`SCP_DIR_ISOLATE_POP`** | Close the active isolate and return to its parent (previous frame). |

Table of the allocated word values (power-of-two offsets for fast decoding):

| Bitmask offset | Directive |
|---------------|-----------|
| `0x00110100 + 0x01` | LTR |
| `0x00110100 + 0x02` | RTL |
| `0x00110100 + 0x04` | ISOLATE_PUSH |
| `0x00110100 + 0x08` | ISOLATE_POP |

> **Reserved:** `0x00110110`–`0x001101FF` remain reserved for future SDF
> directives and MUST NOT be allocated to other subsystems.

---

## 4. Framed Output Words

The SDF single-pass engine emits a **framed word** for every decoded
codepoint. This is the unit consumed by renderers. To carry the full 31-bit
codepoint *plus* framing metadata, the framed word is 64-bit
(`suts32_framed_t`).

```
 63          35 34    31 30                           0
+--------------+---+-----+-----------------------------+
|   reserved   | M | DIR |         codepoint           |
+--------------+---+-----+-----------------------------+
```

| Bits | Field | Width | Meaning |
|------|-------|-------|---------|
| 0–30 | `codepoint` | 31 | The decoded SUCS codepoint. |
| 31–33 | `dir_type` | 3 | Directional classification of this word. |
| 34 | `mirrored` | 1 | Auto-mirror flag — glyph must be mirrored during layout. |
| 35–63 | reserved | 29 | Reserved; MUST be zero. |

> **On the 32-bit SUTF-32 transformation:** the raw SUTF-32 code unit is a
> genuine 32-bit transformation unit carrying the 31-bit codepoint
> (bit 31 always clear). The SDF **framer** augments that unit into the
> 64-bit framed word above during decode. The `mirrored`/`dir_type` bits are
> therefore a property of the *framer output record*, not of the
> transformation code unit itself.

### 4.1 Directional Classification (`dir_type`)

`dir_type` is produced by the Dual-Mode Directional Resolver (§5.3). For
Unicode-Bridge codepoints it is derived from the SUCD BiDi property mask
(§5.4); for native codepoints it inherits the active isolate's resolved
direction.

| Value | `suas_dir_type_t` | Meaning |
|-------|-------------------|---------|
| 0 | `SUAS_DIR_NEUTRAL` | No intrinsic direction; takes the isolate's resolved direction. |
| 1 | `SUAS_DIR_LTR` | Intrinsically left-to-right. |
| 2 | `SUAS_DIR_RTL` | Intrinsically right-to-left. |
| 3 | `SUAS_DIR_MIRRORED_LTR` | Paired/mirrorable glyph, currently resolving LTR. |
| 4 | `SUAS_DIR_MIRRORED_RTL` | Paired/mirrorable glyph, currently resolving RTL. |

### 4.2 Mirroring

`mirrored=1` marks the word as a **paired-format glyph** (e.g. `(`/`)`,
`[`/`]`, `{`/`}`) whose appearance is direction-sensitive. It is set when the
SUCD BiDi mask for the codepoint carries `SUCD_BIDI_MIRRORED`. During
framebuffer layout, a renderer that sees `mirrored=1` with `dir_type`
resolving to an RTL context MUST use the mirrored counterpart of the glyph.
LTR context uses the canonical counterpart. This is a single per-glyph
decision — no reorder pass.

---

## 5. The Single-Pass State Machine

The SDF engine is a deterministic state machine that consumes one codepoint
at a time from a SUTF-32 (or SUTF/SUST-decoded) stream and appends a framed
word to a caller-provided output buffer.

### 5.1 States

The machine has three explicit operational states (`suas_sdf_runtime_t`,
returned by `suas_sdf_runtime()`):

| State | Meaning |
|-------|---------|
| `SUAS_SDF_RUNTIME_ACTIVE` | Decoding normal content within the current isolate. |
| `SUAS_SDF_RUNTIME_ENDED` | `suas_sdf_finish()` was called; no further input is accepted. |
| `SUAS_SDF_RUNTIME_ERROR` | A structural error occurred (stack overflow/underflow, argument error, truncated stream). |

### 5.2 Per-Codepoint Dispatch (`suas_sdf_process_codepoint`)

Every decoded codepoint is routed through the dispatch function
`suas_sdf_process_codepoint()`. Dispatch is by **address-space zone**:

| Zone | Range | Mode | Handling |
|------|-------|------|----------|
| Unicode Bridge | `0x00000000`–`0x0010FFFF` | **Implicit Fallback** | Query SUCD BiDi mask (§5.4) to classify `dir_type`/`mirrored`. |
| SCP | `0x00110000`–`0x0011FFFF` | **Explicit Directorial** | If a directional directive (§3): mutate the isolate stack immediately, emit nothing. Any other SCP control codepoint: emit a neutral, non-advancing control word. |
| Native SUCS | `0x00120000`–`0x7FFFFFFF` | **Inherit** | Adopt the active isolate's resolved direction; emit a neutral/native word. |

The precise steps are:

1. If `cp` is an **SCP directional directive**, update the isolate stack per
   §3 and emit **no** framed word:
   - `LTR` → set active `cur_dir = LTR`.
   - `RTL` → set active `cur_dir = RTL`.
   - `PUSH` → if stack full → `SUAS_SDF_ERR_STACK_OVERFLOW`; else push
     (`base = cur_dir`, `cur = cur_dir`) and make it active.
   - `POP` → if depth 0 (no isolate to pop) → `SUAS_SDF_ERR_STACK_UNDERFLOW`;
     else pop and re-activate the parent.
2. If `cp` is in the **SCP** but not a directive, emit a neutral control word
   (no cursor advance).
3. If `cp` is in the **Unicode Bridge**, query the SUCD BiDi mask and derive
   `dir_type` + `mirrored` (see §5.4).
4. If `cp` is in **Native SUCS**, adopt the active isolate's resolved
   direction (no SUCD classification).
5. Emit the framed word bound to the resulting direction.

### 5.3 Dual-Mode Directional Resolver

The resolver selects direction semantics by zone as table below. This is the
single source of truth for how a codepoint's `dir_type` and `mirrored` are
computed; renderers MUST NOT re-derive direction with their own heuristics.

| Zone | Resolver Mode | Direction Source |
|------|---------------|------------------|
| Unicode Bridge | **Implicit Fallback** | SUCD BiDi property mask (§5.4). |
| SCP (directives) | **Explicit Directorial** | The four SCP directives only. |
| SCP (other) | Static | Neutral, non-advancing control. |
| Native SUCS | **Inherit** | Active isolate stack `cur_dir`. |

### 5.4 SUCD BiDi Property Integration

For Unicode-Bridge codepoints, direction is read from the **SUCD**
(SuperUnicode Character Database) per-codepoint BiDi property bits. The mask
is a bitfield; a codepoint may carry several orthogonal flags:

| Mask | Semantics |
|------|-----------|
| `SUCD_BIDI_LTR` | Strong left-to-right. |
| `SUCD_BIDI_RTL` | Strong right-to-left. |
| `SUCD_BIDI_ARABIC_AL` | Arabic letter — resolves right-to-left. |
| `SUCD_BIDI_NEUTRAL` | Direction-neutral; inherits isolate direction. |
| `SUCD_BIDI_MIRRORED` | Paired-format glyph; must be auto-mirrored. |
| `SUCD_BIDI_WHITESPACE` | Separator / whitespace; neutral. |

`suas_sucd_bidi(cp)` returns the bitmask for a codepoint. The normative table
lives in `sucd/Props/BidiProps.txt`; the reference implementation embeds a
compiled subset (`suas_sucd_bidi`) sufficient for the documented scripts
(Latin, Arabic, Hebrew) and mirrorable punctuation.

Resolution precedence when deriving `dir_type` from the mask:

1. If `SUCD_BIDI_MIRRORED` set → `MIRRORED_LTR`/`MIRRORED_RTL` per active
   isolate direction; `mirrored = 1`.
2. Else if `SUCD_BIDI_ARABIC_AL` or `SUCD_BIDI_RTL` → `RTL`.
3. Else if `SUCD_BIDI_LTR` → `LTR`.
4. Else (`NEUTRAL`/`WHITESPACE`/omitted) → `NEUTRAL`.

Native SUCS codepoints bypass the mask and inherit the active isolate
direction (`SUAS_DIR_NEUTRAL`, resolved at render from `cur_dir`).

### 5.5 Full Bidirectional Processing Model (UAX #9 semantics)

The single-pass framer (§5.2) is the **decode-time framing layer**: it tags
every codepoint with `dir_type`/`mirrored` in logical order. It does not, by
itself, produce a visual line. For renderers and text engines that need full
lexical bidirectional layout—including explicit embeddings, overrides,
isolates, weak/neutral resolution and final reordering—SDF exposes a complete
**paragraph resolver** (`suas_sdf_resolve_paragraph`) implementing the
Unicode Bidirectional Algorithm (UAX #9) semantics directly, in freestanding
C99 with zero heap. All of the rules below operate on caller-provided
arrays sized by `SUAS_SDF_BIDI_MAX_LEN`.

This is the same engine family as the framer; it is a *display-space*
companion to the *decode-space* framer rather than a competing model. Both
cite SUCD BiDi properties as their single source of truth.

#### 5.5.1 Bidirectional Character Types

The resolver classifies every Unicode-Bridge codepoint into one of the
twenty-three **bidirectional character types** (table `suas_sdf_bidi_class_t`):

| Type | Meaning |
|------|---------|
| `L`, `R`, `AL` | Strong: left-to-right, right-to-left, arabic letter |
| `EN`, `ES`, `ET`, `AN`, `CS`, `NSM`, `BN` | Weak |
| `B`, `S`, `WS`, `ON` | Neutral |
| `LRE`, `LRO`, `RLE`, `RLO`, `PDF` | Explicit embedding / override controls |
| `LRI`, `RLI`, `FSI`, `PDI` | Explicit isolate controls |

The implicit directional marks LRM/RLM/ALM resolve exactly as their
corresponding strong characters and are zero-width in display. The explicit
formatting characters are defined by codepoints in §5.5.2; SCP directives and
native codepoints classify as `L`.

#### 5.5.2 Explicit Directional Formatting Characters

SDF recognizes the UAX #9 explicit directional formatting characters in the
Unicode Bridge:

| Abbr | Codepoint | Role |
|------|-----------|------|
| LRM | `U+200E` | Left-to-right mark (strong L, zero-width) |
| RLM | `U+200F` | Right-to-left mark (strong R, zero-width) |
| ALM | `U+061C` | Arabic letter mark (strong AL, zero-width) |
| LRE | `U+202A` | Left-to-right embedding |
| RLE | `U+202B` | Right-to-left embedding |
| PDF | `U+202C` | Pop directional formatting |
| LRO | `U+202D` | Left-to-right override |
| RLO | `U+202E` | Right-to-left override |
| LRI | `U+2066` | Left-to-right isolate |
| RLI | `U+2067` | Right-to-left isolate |
| FSI | `U+2068` | First strong isolate |
| PDI | `U+2069` | Pop directional isolate |

These are the in-band explicit controls. The SCP directional directives
(§3) remain the SCP-native explicit mechanism; both converge on the same
directional-status stack model.

#### 5.5.3 Rule Sequence

The resolver applies the UAX #9 normative rule sequences in order:

- **P1–P3 — Paragraph level.** The text is separated into paragraphs at `B`
  separators. The paragraph level is `1` (RTL) if the first strong character
  (scanning per P2) is `R` or `AL`, else `0` (LTR); it may be forced LTR/RTL
  (`SUAS_SDF_PARA_LTR`/`_RTL`).
- **X1–X8 — Explicit embedding levels.** A directional-status stack (level,
  directional override status, directional isolate status) is driven by the
  explicit formatting characters, with overflow-isolate, overflow-embedding
  and valid-isolate counters (BD3) protecting the fixed stack.
- **X9 — Removal.** RLE, LRE, RLO, LRO, PDF and BN are flagged `removed`;
  isolate initiators and PDI are retained as neutral, zero-width characters.
- **W1–W7 — Weak types.** NSM adopts the preceding type; EN/AN/ES/ET/CS/ON
  resolution follows the numeric sequences and the last-strong rules.
- **N0–N2 — Neutral types + bracket pairs.** Paired brackets (BD14–BD16)
  resolve from enclosing or internal strong direction; remaining neutrals
  take the embedding direction.
- **I1–I2 — Implicit levels.** R→odd, L→even, AN/EN→two above even / one
  above odd.
- **L1–L4 — Reordering + mirroring.** Trailing whitespace resets to the
  paragraph level; the L2 reversal produces **visual order** as an index
  array; L3 shaping is deferred; L4 mirrors pair-format glyphs in RTL
  contexts.

Each codepoint yields a `suas_sdf_run_t` (`cp`, `cls`, `level`, `mirrored`,
`removed`). The `level` field is the resolved embedding level (0–126); the
`visual` output array maps logical positions to display order. Mirroring
(L4) substitutes the mirrored counterpart and sets `mirrored=1` for
Bidi_Mirrored glyphs in odd-level (RTL) contexts.

#### 5.5.4 Zero-Heap Guarantee

`suas_sdf_resolve_paragraph`, like the framer, performs no dynamic
allocation: classification, the embedding-level stack, and reordering all
operate on fixed caller-provided buffers. Inputs exceeding
`SUAS_SDF_BIDI_MAX_LEN` are rejected with `SUAS_SDF_BIDI_ERR_TOO_LONG`.

---

## 6. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| Single-pass consumption of input | Yes | Distinguish active/ended/error states; no re-read of input. |
| Zero heap allocation | Yes | No `malloc`/`realloc`/`free` in implementation. |
| `O(1)` stack push/pop on fixed array | Yes | Stack is a fixed array inside the framer. |
| Overflow/underflow are structural errors | Yes | `suas_sdf_process_codepoint()` reports `SUAS_SDF_ERR_STACK_OVERFLOW` / `_UNDERFLOW`. |
| Mirrored + dir_type on every framed word | Yes | Framed word encoding per §4. |
| Dual-mode direction resolution | Yes | Unicode Bridge → SUCD BiDi; SCP directives; Native → inherit (§5.3). |
| Full bidirectional processing model | Yes | P1–P3, X1–X8, X9, W1–W7, N0–N2, I1–I2, L1–L4 via `suas_sdf_resolve_paragraph` (§5.5). |
| Explicit-only scope changes | Yes | Push/pop/switch only via the four directives (§3); explicit formatting chars (LRE/RLE/LRO/RLO/PDF, LRI/RLI/FSI/PDI) drive the directional-status stack (§5.5). |
| Deterministic output | Yes | Same input ⇒ same framed sequence. |
| Freestanding C99 | Yes | Compiles under `-std=c99 -ffreestanding`. |

---

## 7. Terms & Conventions

- **SCP** — System Control Plane (`0x00110000`–`0x0011FFFF`).
- **SUTF** — SuperUnicode Transformation Format (codepoint ⇄ code units).
- **SUST** — SuperUnicode Serialization Transport (physical bytes/frames).
- **SUCD** — SuperUnicode Character Database (authoritative codepoint
  properties; BiDi masks in `sucd/Props/BidiProps.txt`).
- **Isolate** — a self-contained directional scope (one stack frame).
- **Framed word** — the renderer-facing output record (§4).
- **Dual-Mode Directional Resolver** — the zone-dispatched direction
  authority of §5.3.
- **UAX #9 / UBA** — the Unicode Bidirectional Algorithm; SDF implements its
  full processing model as §5.5.
- **Bidirectional character type** — the UAX #9 Bidi_Class value of a
  codepoint (§5.5.1).
- **Embedding level** — a 0–126 integer capturing nesting depth and default
  direction (even=L, odd=R); produced by X1–X8/I1–I2.
- **Isolating run sequence** — the X10 unit over which W/N/I rules apply. A
  **level run** is a maximal run of equal embedding levels.
- **Directed isolate** — LRI/RLI/FSI ... PDI scope (BD8–BD9, BD12–BD13).

---

**END OF SUAS-001**
