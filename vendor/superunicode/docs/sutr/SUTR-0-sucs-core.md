# SUTR-0 — SUCS Core

**Standard ID:** SUTR-0
**Status:** 0.1.0 — Published
**Scope:** Level 2 (Coded Character Set) — the foundational layout of Base SuperUnicode
**Applies To:** every SuperUnicode consumer; all SUTF forms, SUST transports, SUAS and SUTS documents
**Last Updated:** 2026-09-30

---

## 1. Overview

SUTR-0 defines the **Base SUCS codepoint space**: its representation, its
hierarchy, its three constituent spaces, and the reserved region at its top.
It is the document every other SUTR report assumes.

SuperUnicode uses codepoints not only as character identities but also as
**machine instructions** — a kernel damage-control directive is a codepoint,
and its numeric value selects the handler. The Base space is therefore larger
than any character repertoire could need, and deliberately partitioned.

```text
SUCS_CP        uint32_t, 31 significant bits
range          0x00000000 – 0x7FFFFFFF
hierarchy      codepoint < plane < district < zone
```

### 1.1 The address hierarchy

The 31 bits partition into **four fixed-width fields**, each a contiguous span
of the one above. This is the bit-addressed hierarchy, and it is the only part
of the hierarchy the implementation defines (`sucs_plane.h`):

| Field | Bits | Extraction macro | Values | Span of one unit |
|-------|------|------------------|--------|------------------|
| Zone | 24–30 (7) | `SUCS_GET_ZONE` | 128 | 256 districts = 2^24 codepoints |
| District | 16–23 (8) | `SUCS_GET_DISTRICT` | 256 per zone | 256 planes = 2^16 codepoints |
| Plane | 8–15 (8) | `SUCS_GET_PLANE` | 256 per district | 256 codepoints |
| Offset | 0–7 (8) | `SUCS_GET_OFFSET` | 256 per plane | 1 codepoint |

Expressed as a product: **128 zones × 256 districts × 256 planes × 256
offsets = 2^31**. The hierarchy is uniform and total; what makes the space
*usable* is that it is not uniformly owned (§4).

### 1.2 Semantic groupings are not address fields

Two further groupings appear throughout the series but are **not** bit
partitions of the codespace, and conflating them with the table above is an
error:

| Grouping | Nature | Declared by |
|----------|--------|-------------|
| **Block** | A *named* run of codepoints, of variable length | `Blocks.txt` — a registry entry, not a size |
| **Range** | A *declared* allocation, of variable length | A plugin's range table (SUTR-5); collisions are rejected at gate time |

A Block and a Range are **open-ended by design**: they are meaningful at any
size, and nothing in the address math depends on them. That is precisely why
they cannot be address fields — the four bit-fields above must tile the space
exactly, and a variable-length grouping cannot. The SCP is located by
coordinate (`District 0x11`, `Plane 0x00` for `0x00110000`), and the
BANcode registry is located by **value** against a range macro, not by walking
the hierarchy.

So the correct chain is `codepoint < plane < district < zone`, with Block and
Range as orthogonal semantic overlays. A document that writes
`codepoint < block < range < plane < district < zone < territory` has
invented two address levels that do not exist.

---

## 2. Scope

SUTR-0 defines the coded character set itself — the mapping from abstract
characters to integers, and the structure of that integer space. It does not
define how codepoints are framed (SUTR-1, SUTF), serialized (SUTR-4, SUST),
stored (SUTR-3), or extended beyond 31 bits (SUTR-5, plugin space). It does
not define the abstract characters themselves (SUTR-7, Level 1 / ACR).

The one guarantee SUTR-0 makes about Unicode — that `0x000000`–`0x10FFFF` is
1:1 with Unicode, permanently — is specified in full by SUTR-6 and summarized
here only as the ownership boundary of §3.

---

## 3. Definitions

- **ED1 — `SUCS_CP`.** The Base SUCS codepoint type: a `uint32_t` carrying 31
  significant bits.
- **ED2 — Sentinel.** `0x7FFFFFFF`. The single in-band invalid value. It is
  never allocated, and it terminates a Base stream.
- **ED3 — Kernel Security Trap.** One of 15 reserved slots in
  `0x7FFFFFF0`–`0x7FFFFFFE`, each governing 128 consecutive **B+** BANcodes.
- **ED4 — BANcode.** A System Control Plane directive codepoint, or (context
  permitting) the trap slot its resolution targets.
- **ED5 — Zone.** The outermost bit-addressed unit; 128 in Base SUCS, from
  bits 24–30.
- **ED6 — Open repertoire.** An abstract character repertoire that grows
  without renumbering. Base SUCS is open; the Unicode Bridge inside it is
  open, and the Native space is open.
- **ED7 — Validity.** `sucs_is_valid(cp)` is true for
  `0x00000000`–`0x7FFFFFEF` and false for the 16 values
  `0x7FFFFFF0`–`0x7FFFFFFF`.

---

## 4. The Three Spaces

The 31-bit space is partitioned into three spaces with different owners and
different rules. This is the only place where SuperUnicode departs from a
flat, uniform codespace.

| Space | Range | Owner and rule |
|-------|-------|----------------|
| **Unicode Compatibility Space** | `0x000000`–`0x10FFFF` | 1:1 with Unicode, permanently. New Unicode codepoints appear at the same numeric values. Never remapped. (SUTR-6) |
| **System Control Plane (SCP)** | `0x110000`–`0x11FFFF` | Non-printable, in-band kernel directives. Contains the BANcode Registry. Not text. |
| **Native SuperUnicode Space** | `0x120000`–`0x7FFFFFFF` | OpenWindows native allocations, reached through RNUR coordinates and RCCR tags. |

The classification is exposed as `sucs_codepoint_type_t`:

| Constant | Value | Domain |
|----------|-------|--------|
| `SUCS_TYPE_UNICODE_COMPAT` | 0 | `0x00000000`–`0x0010FFFF` — Unicode Parity Zone |
| `SUCS_TYPE_SYS_FUNCTION` | 1 | `0x00110000`–`0x0011FFFF` — System Control Plane |
| `SUCS_TYPE_NATIVE_ALLOC` | 2 | `0x00120000`–`0x7FFFFFFF` — Native Extended Allocations |
| `SUCS_TYPE_INVALID` | 3 | outside the 31-bit Base address space |

### 4.1 Non-Contiguity in Use

The codespace is contiguous in span but deliberately non-contiguous in *use*.
The UTF-16 surrogate range `0xD800`–`0xDFFF` is **not** excluded: SuperUnicode
has no surrogates, so those 2,048 codepoints are ordinary valid codepoints
inside the Native space and are allocated like any other. An implementation
MUST NOT carve them out.

---

## 5. The System Control Plane

The SCP is a 64K-codepoint space of machine instructions carried *in band*,
in the same codepoint stream as text. Its registry is partitioned into four
severity clusters:

| Cluster | Range | Count | Purpose |
|---------|-------|-------|---------|
| **B+ BANcode** | `0x0011A000`–`0x0011A7FF` | 2,048 | Unrecoverable critical panics, hardware faults, fatal exceptions |
| **W+ WARNcode** | `0x0011A800`–`0x0011ABFF` | 1,024 | Non-fatal telemetry; threshold alerts, degraded performance, predictive maintenance |
| **C+ COMcode** | `0x0011AC00`–`0x0011ADFF` | 512 | IPC events, bus orchestration, hardware handshakes, driver attachment, control-plane signals; also success reports |
| **S+ SOFTcode** | `0x0011AE00`–`0x0011AEFF` | 256 | Soft recovery, fault containment, cache reconciliation, self-healing; also soft errors |

The BANcode Registry spans `0x0011A000`–`0x0011AEFF` (3,840 codepoints) and
all slots are unassigned at v0.1.0.

### 5.1 Trap Dispatch Geometry

```
SUCS_TRAP_SLOT_COUNT     15
SUCS_BANCODES_PER_TRAP   128
```

When the kernel faults it raises a **B+ BANcode**. `sucs_bancode_to_trap()`
maps a B+ codepoint to one of 15 trap slots in `0x7FFFFFF0`–`0x7FFFFFFE`:

```c
slot = (bancode_cp - SUCS_BANCODE_RANGE_MIN) / SUCS_BANCODES_PER_TRAP;
trap = SUCS_KERNEL_TRAP_MIN + slot;
```

`sucs_trap_to_bancode_range()` inverts it. The dispatch is O(1) — one
subtract, one divide by a compile-time constant, one add.

Three properties of this mapping are normative, and all three are load-bearing:

- **B+ only.** The mapping is defined for `0x0011A000`–`0x0011A7FF` and
  **nothing else**. W+, C+ and S+ codepoints have **no trap**; they are
  classified (`sucs_is_warncode()` etc.) but are never dispatch targets. A
  trap-bearing event is a fatal B+ event by construction.
- **Partial coverage.** 15 slots × 128 = 1,920 governed codepoints, but B+
  contains 2,048. The topmost 128 B+ codepoints, `0x0011A780`–`0x0011A7FF`,
  are therefore **unmapped**: `sucs_bancode_to_trap()` returns
  `SUCS_INVALID_CODEPOINT` for them. The registry is 15 slots wide, not 16,
  and the arithmetic says so.
- **Invertibility is total.** Every one of the 15 traps maps back to exactly
  one 128-codepoint B+ range. `sucs_trap_to_bancode_range()` returns `false`
  for the Sentinel, for values outside the trap range, and for null outputs.

Because SCP codepoints are in-band *addresses*, they must survive canonical
transformation untouched and in their original stream position. SUCF (SUAS-004)
guarantees this, so no text pipeline may decompose, reorder, or drop a kernel
directive.

---

## 6. The Terminal Region

The top of the space is reserved and is not allocated:

```text
0x7FFFFFF0 – 0x7FFFFFFE   Kernel Security Traps   (15 slots)
0x7FFFFFFF               Sentinel                 (the in-band invalid value)
```

- **The 16 values from `0x7FFFFFF0` to `0x7FFFFFFF` are never allocated.**
  The top 15 are trap slots; the 16th is the Sentinel. This is what makes the
  Sentinel usable as a terminator, and it is why `0x7FFFFFEF` — not
  `0x7FFFFFFE` — is the last allocatable codepoint in Base SUCS.
- The Sentinel is an **in-band** invalid value. It constrains Levels 3 and 4
  (SUTR-1, SUTR-4): no SUTF form may emit it as data, and every SUST transport
  must be able to represent it as a terminator.
- ExtSUCS dissolves this constraint. The 64-bit space is unbounded, so
  `0x7FFFFFFF` becomes an ordinary codepoint and errors move entirely out of
  band. See SUTR-5.

`sucs_is_valid()` is the normative predicate at every API boundary, and it is
what distinguishes *addressable* from *allocatable*. It rejects the 16 terminal
values, so a decoded stream that yields any of them is corrupt, not text.

---

## 7. Status Return Codes

Operations over the space report through `sues_status_t`:

| Code | Value | Meaning |
|------|-------|---------|
| `SUES_SUCCESS` | 0 | Operation succeeded |
| `SUES_ERR_INVALID_BYTE` | -1 | Malformed input byte |
| `SUES_ERR_BUFFER_TOO_SMALL` | -2 | Output buffer insufficient |
| `SUES_ERR_OUT_OF_BOUNDS` | -3 | Address outside the 31-bit space |
| `SUES_ERR_INVALID_CODEPOINT` | -4 | Sentinel or Kernel Security Trap range |

Note that the trap range is reported as `SUES_ERR_INVALID_CODEPOINT`: the trap
slots are reserved, not allocatable, and a caller must not round-trip one as
data.

---

## 8. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| `SUCS_CP` is 31 bits, span `0x00000000`–`0x7FFFFFFF` | Yes | `sizeof(sucs_char_t)`; `SUCS_MAX_CODEPOINT` |
| Bit hierarchy is Zone 7 / District 8 / Plane 8 / Offset 8 | Yes | `SUCS_GET_*` in `sucs_plane.h` |
| 128 × 256 × 256 × 256 = 2^31 | Yes | Product identity |
| Block and Range are variable-length, not address fields | Yes | `Blocks.txt`; plugin range tables |
| Unicode Compatibility Space is `0x000000`–`0x10FFFF` | Yes | `SUCS_TYPE_UNICODE_COMPAT` classification |
| SCP is `0x110000`–`0x11FFFF` | Yes | `SUCS_TYPE_SYS_FUNCTION` classification |
| BANcode Registry is `0x0011A000`–`0x0011AEFF` | Yes | Range macros; cluster boundaries sum to 3,840 |
| B+ / W+ / C+ / S+ are 2048 / 1024 / 512 / 256 | Yes | Range macros in `sucs_types.h` |
| `0xD800`–`0xDFFF` are valid PUA, never reserved | Yes | Round-trip the whole block |
| Traps are `0x7FFFFFF0`–`0x7FFFFFFE`, exactly 15 | Yes | `SUCS_TRAP_SLOT_COUNT`; `sucs_is_kernel_trap` |
| 128 B+ BANcodes per trap | Yes | `SUCS_BANCODES_PER_TRAP` |
| Trap dispatch covers B+ only | Yes | `sucs_is_bancode()` guard in `sucs_bancode_to_trap()` |
| `0x0011A780`–`0x0011A7FF` is unmapped | Yes | Returns `SUCS_INVALID_CODEPOINT` |
| BANcode ↔ trap mapping is O(1) and invertible | Yes | `sucs_bancode_to_trap()` / `sucs_trap_to_bancode_range()` |
| Valid range is `0x00000000`–`0x7FFFFFEF` | Yes | `sucs_is_valid()`; `0x7FFFFFEF` true, `0x7FFFFFF5` false |
| Sentinel is `0x7FFFFFFF`, never allocated | Yes | `sucs_is_valid()` returns false |
| Terminal 16 values are unallocatable | Yes | `SUCS_INVALID_CODEPOINT` = `0x7FFFFFFF` |
| Trap range reports `SUES_ERR_INVALID_CODEPOINT` | Yes | `sues_status_t` |
| Freestanding C99 | Yes | `-std=c99 -ffreestanding` |

---

## 9. Terms & Conventions

- **SUCS** — SuperUnicode Character System; the 31-bit Base codepoint space.
- **`SUCS_CP`** — the Base codepoint type (ED1).
- **Sentinel** — `0x7FFFFFFF`, the in-band invalid value (ED2).
- **Kernel Security Trap** — one of 15 reserved dispatch slots (ED3).
- **BANcode / WARNcode / COMcode / SOFTcode** — the four SCP severity
  clusters: B+ fatal, W+ warning, C+ command, S+ soft.
- **SCP** — System Control Plane, `0x00110000`–`0x0011FFFF`.
- **Unicode Compatibility Space** — `0x000000`–`0x10FFFF`; see SUTR-6.
- **Native SuperUnicode Space** — `0x120000`–`0x7FFFFFFF`.
- **ExtSUCS** — the unbounded 64-bit space (`sucs_ex_char_t`), which inherits
  this partition and adds the plugin space above the Sentinel.
- **SUES** — SuperUnicode Encoding Status; the `sues_status_t` return codes.
- **RNUR** — Reddit Neographical Unicode Registry; the coordinate system for
  Native-space allocations.
- **RCCR** — Reddit Conlang Code Registry; BCP 47 tags for RNUR-encoded
  conlangs and conscripts.
- **SUCEM** — the four-level model, specified by SUTR-7.

---

**END OF SUTR-0**
