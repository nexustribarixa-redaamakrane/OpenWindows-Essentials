# SUAS-003 — System Boundary & Line Break Rules (SBR)

**Standard ID:** SUAS-003
**Status:** Draft — Ratified
**Scope:** Core Kernel Invariant
**Applies To:** terminal emulators, text editors, word processors, pagination engines, streaming formatters, layout/compositor subsystems
**Last Updated:** 2026-09-05

---

## 1. Overview

System Boundary & Line Break Rules (SBR) is the **core architecture**
governing line breaking in SuperUnicode. It defines where a line may, must, or
must not be broken, over the **full 64-bit SUCS codepoint space** (Base SUCS +
SCP + Native SUCS + ExtSUCS plugins), through a **single-pass deterministic
state transition table**.

SBR is the structural successor to the Unicode Line Breaking Algorithm (UAX
#14, `UAX14.txt` / `LineBreak.txt`). It retains the well-understood
break-class semantics for the Unicode Bridge, then extends the model to the
rest of SUCS and folds it into a single *transition-matrix* contract: two
adjacent codepoints resolve to a break status — **must break, may break, must
not break, or alphanumeric break** — in **constant time**, with no allocation
and no multi-pass buffer preprocessing.

### 1.1 Goals

- **Single-pass deterministic resolution.** Break decisions for a pair of
  adjacent codepoints fall out of the state transition table in O(1), without
  the multi-pass buffer algorithm of UAX #14.
- **Zero allocation.** The engine holds at most an O(1) register/stack of the
  immediately preceding break state; no heap and no growing buffer.
- **Explicit control directives.** Invisible formatting codes (WJ, ZWSP) and
  **Explicit SCP Break Markers** give the author or a protocol absolute
  override over opportunistic (automatic) breaks.
- **Deterministic resolution.** Given the same pair of codepoints, any
  conforming implementation MUST compute an identical break status.
- **Full 64-bit coverage.** All four districts of SUCS/ExtSUCS dispatch
  through the Dual-Mode Break Resolver (§5).

### 1.2 Non-Negotiable Invariants

1. **No heap.** The SBR engine performs no dynamic allocation; all tables are
   immutable static data and all working state is caller-provided.
2. **O(1) pair resolution.** `suas_sbr_pair()` must classify a single adjacent
   pair in constant time, independent of run length.
3. **Three statuses + override.** `MUST_BREAK`, `CAN_BREAK`, `NO_BREAK` —
   plus `ALPHANUM_BREAK` for the CJK/numeric alphabetic mandatory-adjacency
   rule (§5.5).
4. **Dual-Mode Break Resolver.** Unicode Bridge → SUCD 8-bit break-class mask
   + transition matrix; SCP → explicit directive override; Native SUCS →
   high-bit range bitmask (O(1)); ExtSUCS plugin → default (§5.4).
5. **Determinism.** Given the same two codepoints, output is identical across
   conforming implementations.

---

## 2. Scope

SBR concerns **where line boundaries may appear**. It defines:

- The break-classification of every codepoint over the 64-bit space.
- The pair-transition contract (`prev`/`cur`) producing a break status.
- The Explicit SCP Break Markers and their absolute override semantics.
- Zone policies for the non-Bridge SUCS districts.

SBR does **not** define hyphenation, justification, glyph metrics (see
SUAS-002 SGW), or direction resolution (see SUAS-001 SDF). It is the
line-boundary sibling of SGW and the layout sibling of SDF; all three consume
the same 64-bit `sucs_ex_char_t` codepoints.

---

## 3. Description

### 3.1 Break Status Classification

Every ordered pair of *adjacent* codepoints `(prev, cur)` yields exactly one
break status. The four statuses are exhaustive and mutually exclusive:

| Status | Value | Meaning |
|--------|-------|---------|
| `SUAS_BRK_MUST_BREAK` | 0 | A line boundary is mandatory here (e.g. between a control and any following character). |
| `SUAS_BRK_CAN_BREAK` | 1 | A line boundary is *allowed* (opportunistic); the engine or formatter may break here. |
| `SUAS_BRK_NO_BREAK` | 2 | A line boundary is *prohibited* here. |
| `SUAS_BRK_ALPHANUM_BREAK` | 3 | A mandatory break due to alphabetic/CJK numeric adjacency (the CJK numeric rule; UAX #14 LB25 analog). |

The transition matrix stores, for each (previous-class, current-class)
pair, whether a break is allowed. `MUST_BREAK` and `ALPHANUM_BREAK` are
represented as strength levels of the same "may not connect" region, while
`CAN_BREAK`/`NO_BREAK` are the two allowed/disallowed outcomes of every
ordinary pair.

### 3.2 Single-Pass State Transition Model

UAX #14 is a multi-pass buffer algorithm: it resolves pairs, then re-resolves
with context (e.g. `SP x` rules, combining-mark rules, and the alphabetic
rules) across the whole line. SBR **collapses this into a state transition
table**: the resolver keeps a small O(1) register describing the *previous*
break class (and, where needed, a one-slot history of "were we inside a space
run" or "last strong class"), and every new codepoint is classified and
paired against that register. Because the table encodes the context rules
directly (a state keyed on the previous class), a run of arbitrary length
consumes with both constant time and constant memory per step.

Concretely, the engine is `suas_sbr_state_t` holding the previous break
class and a small status flag, advanced by `suas_sbr_process_codepoint()`,
with `suas_sbr_pair()` available as the stateless two-codepoint primitive.

### 3.3 Control Directives

Two mechanisms authoritatively control breaks:

- **Invisible formatting codes** (Unicode Bridge): `WJ` (`U+2060`
  WORD JOINER) and `ZWSP` (`U+200B` ZERO WIDTH SPACE) behave per UAX #14:
  WJ prohibits breaks on either side; ZWSP *creates* a break opportunity.
- **Explicit SCP Break Markers** (control plane): three directives in the
  SCP Break block that **override** any automatic resolution. They are the
  only way to force mandatory or prohibited breaks regardless of the
  surrounding break classes (§6.2).

---

## 4. Definitions

- **ED1 — Break class.** One of the SBR break classes (§5.1).
- **ED2 — Break status.** One of `MUST_BREAK`, `CAN_BREAK`, `NO_BREAK`,
  `ALPHANUM_BREAK` (§3.1).
- **ED3 — Explicit SCP Break Marker.** An SCP directive that forces a break
  outcome independent of break classes (§6.2).
- **ED4 — Line boundary.** The position between two consecutive codepoints
  (and between the start/end of a paragraph and the first/last codepoint).
- **ED5 — Alphanumeric break.** The mandatory CJK/numeric alphabetic
  adjacency rule (§5.5).

---

## 5. Classification & Resolution

### 5.1 Break Classes

SBR recognizes the UAX #14 break-class set for the Unicode Bridge. The
reference implementation curates the following classes (superset visible to
the transition matrix):

`BK, CR, LF, CM, NL, SG, WJ, ZW, GL, SP, ZWJ, B2, BA, BB, HY, CB, CL, CP, EX,
IN, IS, NU, OP, PO, PR, QU, SA, AL, ID, EB, EM, H2, H3, HL, RI, JL, JV, JT,
XX`.

The Dual-Mode resolver stores each class as a small integer; the map used by
the implementation is enumerated in `suas_sbr.h` (`suas_sbr_break_class_t`).

### 5.2 The Transition Matrix

The core contract is the pair matrix

```
Matrix[prev_class][cur_class] -> break status (CAN_BREAK / NO_BREAK)
```

with two distinguished classes producing `MUST_BREAK` (a `BK`, `CR`, `LF`,
`NL`, mandatory control) and `ALPHANUM_BREAK` (§5.5). The matrix encodes the
UAX #14 pair rules (LB4–LB21) and the subsequent context-adjusted classes in
a single immutable table.

### 5.3 Invisible Formatting Codes

- `U+2060 WJ` classes as `WJ` — the matrix blocks a break on either side of a
  `WJ` codepoint (UAX #14 LB16).
- `U+200B ZWSP` classes as `ZW` — the matrix permits a break after `ZW`
  regardless of the following class (LB8/LB8a), provided the previous class
  does not forbid it (SP rule) or the following is not a mandatory-connect.
- The reserved `U+200C ZWNJ` class is `CM`-adjacent handling only; `U+200D
  ZWJ` classes as `ZWJ` and does not offer a break.

### 5.4 Zone Dispatch (64-bit SUCS)

| District | Policy |
|----------|--------|
| Unicode Bridge `0x00000000`–`0x0010FFFF` | Full UAX #14 break-class table + transition matrix (default: `XX`). |
| SCP `0x00110000`–`0x0011FFFF` | **Control plane.** Explicit SCP Break Markers override (§6.2); other SCP codepoints are mandatory controls (`MUST_BREAK` on both sides). |
| Native SUCS `0x00120000`–`0x7FFFFFFE` | **Break-neutral.** High-bit range bitmask test (§5.6) resolves in O(1); default `XX` with the alphabetic/CJK rule active. |
| ExtSUCS plugin `>0x7FFFFFFF` | **Break-neutral** default; a plugin may register property via the tailoring hook (§6.3). |
| Trap range `0x7FFFFFF0`–`0x7FFFFFFE` / sentinel `0x7FFFFFFF` | Not renderable; `NO_BREAK`/terminator. |

### 5.5 Alphanumeric Break (CJK Numeric Rule)

For alphabetic and CJK/ideographic/numeric codepoints (classes `AL`, `HL`,
`ID`, `NU` and native-word codepoints), the CJK numeric rule (UAX #14 LB25
analog) inserts a mandatory break in the pattern `numeric x numeric`,
`numeric +`, `+ numeric` where the `+` is a non-alphabetic separator that
would otherwise glue two numeric fields. SBR exposes this as the
`ALPHANUM_BREAK` status so a formatter can honor the hard CJK convention
without waiting for hyphenation heuristics.

### 5.6 Native SUCS High-Bit Range Bitmask

Native and plugin codepoints do not carry a full break-class table. Instead
the resolver tests the codepoint against a small set of high-bit range
bitmasks — an **O(1)** range test — to decide whether the codepoint is a
native "word" character (alphabetic/ideographic → participates in the
mandatory-adjacency and NO_BREAK-inside-word rules) or a neutral gap
(boundary-eligible). This preserves the zero-allocation, constant-time
contract across the entire space.

---

## 6. Recommendations

### 6.1 Formatter / Renderer Use

- Consume the stream with `suas_sbr_process_codepoint()` and inspect the
  returned status at each step, or use the stateless `suas_sbr_pair()`.
- Respect `MUST_BREAK` unconditionally; use `CAN_BREAK` as a soft opportunity
  subject to measure/justification passes; never break on `NO_BREAK`.
- Treat `ALPHANUM_BREAK` as mandatory for CJK/numeric alphabetic spacing.

### 6.2 Explicit SCP Break Markers

| Codepoint | Directive | Meaning |
|-----------|-----------|---------|
| `0x00110020` | `SCP_BRK_MANDATORY` | Force a line break here (overrides NO_BREAK). |
| `0x00110021` | `SCP_BRK_PROHIBITED` | Force no line break here (overrides CAN_BREAK/MUST under spelling exceptions). |
| `0x00110022` | `SCP_BRK_OPPORTUNISTIC` | Allow a break here (overrides NO_BREAK). |

These are **absolute**: a MANDATORY marker between two otherwise
no-break classes forces `MUST_BREAK`; an OPPORTUNISTIC marker forces
`CAN_BREAK`; a PROHIBITED marker forces `NO_BREAK`. They override any
automatic class result.

### 6.3 Tailoring Hook

As with SGW, SBR exposes a caller-provided override table consulted **before**
the normative classes/table. A protocol or editor may pin ranges of codepoints
to a specific break class or pair outcome without mutating the immutable
default table. When the list is empty (default), the normative model applies.

---

## 7. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| Four break statuses (MUST/CAN/NO/ALPHANUM) | Yes | `suas_sbr_pair()` returns each status. |
| Single-pass O(1) pair resolution | Yes | `suas_sbr_pair()` is constant-time per pair, no multi-pass buffer. |
| Zero heap allocation | Yes | No `malloc`/`realloc`/`free` in implementation. |
| Zone dispatch over 64-bit SUCS | Yes | `suas_sbr_classify()`/state advances by §5.4. |
| Explicit SCP Break Markers override | Yes | `SCP_BRK_MANDATORY/PROHIBITED/OPPORTUNISTIC` absolute. |
| Invisible formatting (WJ, ZWSP) handling | Yes | Matrix rules LB16/LB8a. |
| Native SUCS high-bit bitmask O(1) test | Yes | `suas_sbr_is_native_word()` range bitmask (§5.6). |
| Alphanumeric (CJK numeric) break | Yes | `SUAS_BRK_ALPHANUM_BREAK` (§5.5). |
| Tailoring hook | Yes | `suas_sbr_options_t` override list (§6.3). |
| Freestanding C99 | Yes | Compiles under `-std=c99 -ffreestanding`. |

---

## 8. Terms & Conventions

- **SCP** — System Control Plane (`0x00110000`–`0x0011FFFF`).
- **SUCD** — SuperUnicode Character Database.
- **SUCS** — SuperUnicode Character System; Base SUCS encodes 31-bit codepoints.
- **ExtSUCS** — the 64-bit `sucs_ex_char_t` codepoint space (Base + plugins).
- **UAX #14** — Unicode Line Breaking Algorithm; SBR's Unicode Bridge
  classification and status semantics are modeled on it.
- **Break class** — the per-codepoint line-breaking category (§5.1).
- **Break status** — the pair outcome: MUST/CAN/NO/ALPHANUM (§3.1).
- **Dual-Mode Break Resolver** — the zone-dispatched classifier + matrix
  (§5.4).

---

**END OF SUAS-003**
