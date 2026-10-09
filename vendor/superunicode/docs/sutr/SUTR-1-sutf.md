# SUTR-1 — SuperUnicode Transformation Formats (SUTF)

**Standard ID:** SUTR-1
**Status:** 0.1.0 — Published
**Scope:** Level 3 (Character Encoding Form) of the SuperUnicode Character Encoding Model
**Applies To:** Base SUCS (31-bit), ExtSUCS (64-bit, via vSUTF), text codecs, terminals, IPC channels, storage adapters
**Last Updated:** 2026-09-30

---

## 1. Overview

SUTF is the **SuperUnicode Transformation Format** family: the *Level 3*
(Character Encoding Form, CEF) layer of SUCEM. A CEF maps the integers of a
coded character set — the 31-bit `SUCS_CP` of Base SUCS — to sequences of
**code units**. A *code unit* is an integer of a specified binary width.

The forms defined here are:

| Form | Code unit | Unit count | Range covered |
|------|-----------|-----------|---------------|
| **SUTF-8** | byte (`uint8_t`) | variable, 1–6 | `0x00000000`–`0x7FFFFFFF` |
| **SUTF-16** | 16-bit word | variable, 1–2 | `0x00000000`–`0x7FFFFFFF` |
| **SUTF-4** | 4-bit nibble | fixed 8 (4 bytes) | `0x00000000`–`0x7FFFFFFF` |
| **SUTF-2** | 2-bit symbol frame | fixed 16 (4 bytes) | `0x00000000`–`0x7FFFFFFF` |
| **vSUTF** | byte | variable | full 64-bit ExtSUCS space |

All four Base forms are lossless over the **entire 31-bit space**, not merely
over the Unicode Bridge. vSUTF extends the model to the unbounded 64-bit
ExtSUCS domain and is defined in the `extsutf` library.

### 1.1 Goals

- **Losslessness over the whole codespace.** Every valid Base SUCS codepoint
  round-trips through every Base form. No form is restricted to
  `0x00000000`–`0x10FFFF`.
- **Endian neutrality.** A form is a *transformation*, not a serialization. It
  MUST NOT fix, imply, or depend on a byte order (§5).
- **Uniform vocabulary.** Each form exposes the same three-function API —
  `sutfN_codepoint_length()`, `sutfN_encode_char()`, `sutfN_decode_char()` —
  so a caller can select a form without learning a new interface.
- **Freestanding C99.** The reference implementation compiles under
  `-std=c99 -ffreestanding` with no heap.

### 1.2 Non-Negotiable Invariants

1. **31-bit coverage.** A Base form MUST encode every `sucs_char_t` in
   `0x00000000`–`0x7FFFFFEF`. The 16 terminal values `0x7FFFFFF0`–`0x7FFFFFFF`
   — 15 trap slots and the Sentinel — are not encodable and MUST NOT be
   produced as data.
2. **No surrogates.** SuperUnicode has no surrogate mechanism. The range
   `0xD800`–`0xDFFF` is an ordinary block of valid PUA codepoints and MUST be
   encoded as such — never paired, never reserved, never rejected.
3. **Endian neutrality.** A form MUST yield the same code-unit sequence
   regardless of host byte order (§5).
4. **Canonical framing only.** A decoder MUST reject an encoding that uses
   more code units than the minimum required for the codepoint (§4.1).
5. **Transport independence.** A form MUST NOT define byte order, bit-packing
   across code units, register alignment, or stream framing. Those belong to
   the SUST family (SUTR-4).

---

## 2. Scope

SUTF defines **how many code units** a codepoint becomes and **what they
contain**. It does not define how those units reach memory or a wire. The
distinction is the whole point of the Level 3 / Level 4 split, and is
normative — see §5.

SUTF concerns the *representation of characters*. It does not define glyph
shaping, bidirectional layout, line breaking, width metrics, or collation;
those are SUAS-001 (SDF), SUAS-002 (SGW), SUAS-003 (SBR), and SUTS-001 (SUCA)
respectively. SUTF-7 (SUCEM) places this document in the full model.

---

## 3. Definitions

- **ED1 — Code unit.** The minimal bit combination representing one unit of
  encoded text, of a width fixed by the form: a byte, a 16-bit word, a 4-bit
  nibble, or a 2-bit symbol frame.
- **ED2 — Form (CEF).** A reversible mapping from CCS integers to sequences of
  code units, with no byte-order decision (§5).
- **ED3 — Transport (CES).** A reversible mapping from code-unit sequences to
  serialized byte sequences, including byte order and framing. Defined by the
  SUST family, not here.
- **ED4 — Canonical framing.** The unique encoding of a codepoint that uses the
  minimum number of code units for its magnitude. Any longer encoding is
  overlong and invalid.
- **ED5 — Sentinel.** `0x7FFFFFFF`. The single in-band invalid codepoint. It
  terminates a Base stream and is never allocated.
- **ED6 — Overlong form.** An encoding of a codepoint using more code units
  than ED4 requires. Always rejected.
- **ED7 — vSUTF.** The variable multi-byte streaming form covering the 64-bit
  ExtSUCS domain, defined in `extsutf`.

---

## 4. The Forms

### 4.1 SUTF-8 — Byte Stream Transformation

SUTF-8 is the general-purpose form: byte-oriented, self-delimiting, and
standard UTF-8 compatible across the Unicode Bridge. It extends UTF-8 to the
full 31-bit SUCS space by continuing the leading-byte pattern to six bytes.

| Length | Codepoint range | Leading bytes |
|--------|-----------------|---------------|
| 1 | `0x00000000`–`0x0000007F` | `0xxxxxxx` |
| 2 | `0x00000080`–`0x000007FF` | `110xxxxx 10xxxxxx` |
| 3 | `0x00000800`–`0x0000FFFF` | `1110xxxx 10xxxxxx 10xxxxxx` |
| 4 | `0x00010000`–`0x0010FFFF` | `11110xxx 10xxxxxx 10xxxxxx 10xxxxxx` |
| 5 | `0x00110000`–`0x03FFFFFF` | `111110xx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx` |
| 6 | `0x04000000`–`0x7FFFFFFF` | `1111110x 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx 10xxxxxx` |

The five- and six-byte forms are where SUTF-8 departs from UTF-8. They exist
because Base SUCS is 31 bits, not 21: a codepoint above `0x10FFFF` — the
System Control Plane, the Native Space, and everything in between — has no
UTF-8 representation and requires 5 or 6 bytes.

A conforming decoder MUST reject:

- a five-byte sequence denoting a value above `0x10FFFF` (ED6, overlong
  relative to the four-byte form), and
- a six-byte sequence denoting a value above `0x3FFFFFF` (ED6, overlong
  relative to the five-byte form),

so that a codepoint has exactly one valid encoding, per ED4. The reference
decoder enforces both bounds in `sutf8_decode_char()`.

Because SUTF-8 is byte-oriented, it has an identity serialization: there is
no `SUST-8` and no byte-order question to answer. A SUCS codepoint cast to a
byte array *is* an SUTF-8 stream, but note that the cast is itself a
serialization decision — see the caution in §5.

### 4.2 SUTF-16 — 16-Bit Word Transformation

SUTF-16 covers the entire 31-bit space in at most two words, using a marker
bit rather than a surrogate range:

| Words | Codepoint range | Shape |
|-------|-----------------|-------|
| 1 | `0x00000000`–`0x00007FFF` | payload word, top bit clear |
| 2 | `0x00008000`–`0x7FFFFFFF` | marker word, then payload word |

The consequence is the invariant most often misread: **`0xD800`–`0xDFFF` are
ordinary codepoints.** They are encoded as two words like any other value
above `0x7FFF`, and they are never paired into a supplementary character.
The word framing is a *length* signal, not a *value* signal.

SUTF-16 defines words, not bytes. Turning the word sequence into a byte
stream requires a byte-order decision, which is **SUST-16** (§5).

### 4.3 SUTF-4 — 4-Bit Hex Nibble Transformation

SUTF-4 encodes every codepoint as a fixed **8 nibbles (4 bytes)**, rendered
as hex digits. It is not a compact form; it is a *debuggable* one, for console
dumps, terminal logging, and low-level bus tracing where a human-readable
fixed-width field matters more than density. The form is fixed-width, so
`sutf4_codepoint_length()` is constant and the stream is trivially seekable.

### 4.4 SUTF-2 — 2-Bit Symbol Frame Transformation

SUTF-2 encodes every codepoint as a fixed **16 frames (4 bytes)** of 2 bits.
It trades the widest form for the narrowest, targeting compressed inter-thread
IPC channels and bitstream links where per-codepoint overhead dominates. As
with SUTF-4, the length is constant and the stream is positionally addressable.

### 4.5 vSUTF — Variable Multi-Byte Streaming Form

vSUTF (`extsutf` / `vsutf.h`) extends the model to the **64-bit ExtSUCS**
domain, which is unbounded and therefore cannot be covered by a
fixed-width form. It is variable-length and streaming, with a fast path for
Base SUCS. It is the only form in the family that is not lossless over a
bounded 31-bit space by construction.

---

## 5. The CEF / CES Boundary (Normative)

The separation of `sutf/` and `sust/` into distinct libraries is not an
organizational convenience. It follows directly from one question:

> **If the processor byte order changed, would this format change?**
>
> - **No** → Level 3, a **CEF**. SUTF-8, SUTF-16, SUTF-4, SUTF-2, vSUTF.
> - **Yes** → Level 4, a **CES**. SUST-16, SUST-32/64/128/256/512/N, e-SUST.

A CEF is a pure function of the codepoint. A CES adds a serialization policy.
The two compose as a product — one CEF with many CESs, one CES carrying many
CEFs — and they have genuinely different invariants: a CEF must be lossless
and order-agnostic, while a CES must be byte-exact and order-explicit.

This has three practical consequences:

1. **A fixed 32-bit form is not an SUTF form.** Fixed-width 32-, 64-, 128-,
   256-, 512-bit and arbitrary *N*-word framings are `SUST-32/64/128/256/512/N`,
   defined in `sust/include/sustfixed.h`. There is no `SUTF-32`.
2. **Little-endian is not an SUTF form.** `SUTF-16` is endian-neutral. The
   byte order is chosen by `SUST-16`, whose canonical order is big-endian and
   which also provides a little-endian variant selected by entry point. Either
   way the order is a *transport* attribute, not a property of the form;
   there is no `SUTF-16-LE`.
3. **A narrow cast is a serialization.** Reinterpreting a `sucs_char_t` or a
   `uint32_t` as `uint8_t[]` produces a byte array in host order. That is a
   Level 4 operation performed implicitly. Code that needs a portable byte
   stream MUST call a SUST transport explicitly.

A test suite that passes for a CEF says nothing about a CES, and vice versa.
Keeping them in separate libraries makes "is this artifact portable?" a
question about the type, not about reading the implementation.

---

## 6. Kernel Mode Switching

`sucs_mode.h` governs which family of forms is live. Because the mode governs
the encoding and the transport together, changing it is a **kernel restart**,
not a runtime switch.

| Mode | Encoding | Forms | Transports |
|------|----------|-------|------------|
| `SUCS_MODE_BASE` (0) | 31-bit Base SUCS | SUTF-8/16/4/2 | SUST-16, SUST-32/64/128/256/512/N |
| `SUCS_MODE_EXTENDED` (1) | unbounded 64-bit ExtSUCS | vSUTF | SUST-64..512, e-SUST |

A mode alteration is staged with `sucs_request_mode_switch()` and committed
during early boot by `sucs_commit_mode_on_boot()`. Runtime callers receive
`SUCS_SWITCH_REBOOT_REQUIRED` rather than a silent transition, because live
data already framed under the previous form would otherwise be reinterpreted
under the next one.

---

## 7. Recommendations

### 7.1 Choosing a Form

- **Default to SUTF-8** for text interchange, file storage, and anything that
  must interoperate with UTF-8 consumers. The Unicode Bridge is byte-identical
  to UTF-8, so bridge-only content needs transcoding nowhere.
- **Choose SUTF-16** when the consumer is word-addressed and the data is
  mostly BMP-range; the fixed word stride suits word-at-a-time processing.
- **Choose SUTF-4 or SUTF-2** only for the debugging and narrow-channel cases
  in §4.3–§4.4. Both cost 4 bytes per codepoint at any magnitude, so they are
  strictly worse than SUTF-8 for storage.
- **Choose vSUTF** only when the domain actually extends above
  `0x7FFFFFFF`; otherwise the Base forms are simpler and faster.

### 7.2 Portability Discipline

- Always state the form *and* the transport when specifying a wire format.
  "SuperUnicode encoded" is not a complete specification.
- Prefer the canonical big-endian `SUST-16` for anything persisted or
  exchanged, and treat little-endian as a machine-local optimization.
- Reject the Sentinel at the API boundary. `sucs_is_valid()` is the
  normative predicate; a decoded stream that yields `0x7FFFFFFF` is corrupt,
  not text.

---

## 8. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| 31-bit lossless coverage, all four Base forms | Yes | Round-trip every boundary value; `test_sutf_all` |
| SUTF-8 spans 1–6 bytes with the §4.1 boundaries | Yes | `sutf8_codepoint_length()` at each threshold |
| SUTF-8 rejects 5-byte forms above `0x10FFFF` | Yes | `sutf8_decode_char()` returns 0 |
| SUTF-8 rejects 6-byte forms above `0x3FFFFFF` | Yes | `sutf8_decode_char()` returns 0 |
| Unique encoding per codepoint (ED4) | Yes | Overlong-form rejection |
| No surrogates; `0xD800`–`0xDFFF` valid PUA | Yes | Encode/decode the whole `0xD800`–`0xDFFF` block |
| SUTF-16: 1 word ≤ `0x7FFF`, 2 words above | Yes | `sutf16_codepoint_length()` at `0x7FFF`/`0x8000` |
| SUTF-4 fixed 8 nibbles (4 bytes) | Yes | `sutf4_codepoint_length()` constant |
| SUTF-2 fixed 16 frames (4 bytes) | Yes | `sutf2_codepoint_length()` constant |
| Endian neutrality (ED3) | Yes | Identical code-unit output on both orders |
| Sentinel never produced as data | Yes | `sucs_is_valid()` gate on every encode |
| No byte order / framing defined here | Yes | No CES symbol appears in `sutf/` |
| Freestanding C99, no heap | Yes | `-std=c99 -ffreestanding`, no `malloc` |
| Mode switch requires kernel restart | Yes | `SUCS_SWITCH_REBOOT_REQUIRED` |

---

## 9. Terms & Conventions

- **SUTF** — SuperUnicode Transformation Format; the Level 3 CEF family.
- **CEF** — Character Encoding Form (SUTM Level 3). Endian-neutral.
- **CES** — Character Encoding Scheme (SUTM Level 4). Byte order + framing.
- **SUST** — SuperUnicode Serialization Transport; the CES family of SUTR-4.
- **SUCS** — SuperUnicode Character System; 31-bit Base (`SUCS_CP`).
- **ExtSUCS** — the unbounded 64-bit space (`sucs_ex_char_t`).
- **Code unit** — the minimal bit combination of a given width (ED1).
- **Sentinel** — `0x7FFFFFFF`, the in-band invalid codepoint (ED5).
- **Overlong form** — a non-minimal encoding of a codepoint (ED6).
- **UTF-8** — the 1–4 byte form covering `0x0000`–`0x10FFFF`; a strict prefix
  of SUTF-8 over the Unicode Bridge.
- **UAX #29** — Unicode Text Segmentation; consumes SUTF streams and governs
  canonical transformation, including the rule that SCP directives pass
  through normalization untouched.

---

**END OF SUTR-1**
