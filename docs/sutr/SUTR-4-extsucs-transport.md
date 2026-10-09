# SUTR-4 — ExtSUCS Transport

**Standard ID:** SUTR-4
**Status:** 0.1.0 — Published
**Scope:** Level 4 (Character Encoding Scheme / Transport) — serialization of codepoints to bytes
**Applies To:** vector storage, IPC, plugin space, page-mapped virtualization
**Last Updated:** 2026-09-30

---

## 1. Overview

SUTR-4 defines the **SUST** family: the SuperUnicode Serialization Transports.
These are the **Level 4 (CES)** implementations of SUCEM — they take sequences
of code units and produce serialized bytes, and they are the only layer in
SuperUnicode that has an opinion about byte order, alignment, and framing.

This report covers two things:

1. The **Base SUCS transports** — `SUST-16` and the fixed-width family
   `SUST-32/64/128/256/512/N` — which serialize 31-bit codepoints.
2. The **ExtSUCS transports** — **vector**, **vSUTF**, and **e-SUST** — which
   carry the unbounded 64-bit domain.

The Level 3 / Level 4 boundary itself is specified in SUTR-1 §5; this report
does not restate it, only applies it.

### 1.1 What a transport is not

A transport is not a *form*. `SUTF-8` (SUTR-1) maps a codepoint to bytes; it is
byte-oriented and endian-neutral. `SUTF-16` maps a codepoint to words; it is
endian-neutral, and becomes bytes only under `SUST-16`. Fixed-width framings
are transports, not forms. An implementation that chooses a transport is making
a **portability** decision; an implementation that chooses a form is not.

---

## 2. Base SUCS Transports

### 2.1 SUST-16

`SUST-16` is the **canonical serialization** of the SUTF-16 word stream: it
defines how 16-bit framing words are packed onto a byte medium — files,
sockets, buses, console dumps. `SUTF-16` defines the codepoint ↔ word
mapping and stops there; `SUST-16` is the layer that makes the bytes.

`SUST-16` provides **two byte orders**, and the distinction is normative:

| Order | Entry points | Status |
|-------|--------------|--------|
| Big-endian | `sust16_encode_bytes()` / `_be()` | **Canonical.** The default; network order. |
| Little-endian | `sust16_encode_bytes_le()` | Available, must be explicitly selected. |

So big-endian is what a spec names when it must name a default, and little-
endian is a *supported variant of the same transport* — selected by entry
point, not by a different format name. There is still no `SUTF-16-LE`:
little-endian is a CES-level choice, never a CEF-level one.

**There is deliberately no byte-order mark.** Every SUTF-16 framing word
`≥ 0x8000` is a marker, so no signature word can exist to carry one. The
consequence is stated plainly in `sust16.h`: the byte order is a transport
attribute **fixed per stream by the sender**, not self-describing, and a
stream decoded under the wrong order either fails loudly (the marker bit
flips) or silently decodes to a different valid codepoint. An `encode_*` MUST
be paired with its matching `decode_*`; hand-rolled packing is the one way to
corrupt a SUTF-16 stream without an error.

### 2.2 The Fixed-Width Family

Fixed-width transports serialize a codepoint into a fixed number of bytes,
zero-padded and big-endian, sized to a natural machine or vector width:

| Transport | Bytes | Alignment | Role |
|-----------|-------|-----------|------|
| `SUST-32` | 4 | 32-bit | Base SUCS fast-path container |
| `SUST-64` | 8 | 64-bit | SIMD / AI tensor slot; full 64-bit ExtSUCS range |
| `SUST-128` | 16 | 128-bit | SSE / NEON vector register slot |
| `SUST-256` | 32 | 256-bit | AVX-256 vector register slot |
| `SUST-512` | 64 | 512-bit | AVX-512 vector register slot |
| `SUST-N` | N×word | arbitrary | General multi-word block container |

`SUST-N` is the general rule; the named widths are conveniences. Because a
fixed-width form is a **transport**, there is no `SUTF-32` and no `SUTF-64` —
those names do not exist in this standard, and code that references them is
referencing a form that was never specified.

Fixed-width transports are **simple** CESs: each code unit maps to a unique
byte sequence, in order, with no shift mechanism and no escaping. There is no
byte-order mark anywhere in the SUST family, because SuperUnicode fixes the
order per transport rather than marking it in the data — the transport *is* the
declaration of order.

---

## 3. ExtSUCS Transports

The 64-bit ExtSUCS domain is unbounded, so it cannot be covered by a
fixed-width form. The Extended transports therefore fall into two shapes: a
fixed-width **vector** (length-prefixed, so a stream can end without a
terminator) and a variable-length **streaming** form.

### 3.1 Vector

A vector is an in-memory sequence of `sucs_ex_char_t` (64-bit, little-endian),
one codepoint per unit:

```text
UNIT  uint64, little-endian, one codepoint per unit
```

**Length is carried by the stream framing, not by the data.** A Base SUCS
stream terminates on the Sentinel `0x7FFFFFFF`; a vector is *length-prefixed*
instead, because a 64-bit space has no reserved terminal value — the plugin
space above the Sentinel is addressable, so the Sentinel cannot double as a
terminator without ambiguity. This is the one place where the Extended domain
forces a different framing convention from the Base.

### 3.2 vSUTF

`vSUTF` is the Extended-domain **transformation format**, and it is a Level 3
artifact in nature: it maps a codepoint to a byte sequence and fixes no
memory layout, alignment, or stream framing. It is specified here because it
sits in the Extended module, but its level classification follows the byte-order
test of SUTR-7 §7 — and vSUTF passes that test as *endian-neutral*, because it
emits bytes directly and never exposes a host word.

Its structure is **not** LEB128, and this is worth stating precisely because
`vsutf.txt` describes it that way while deferring to `vsutf.h` as the
reference. `vsutf.h` is normative. vSUTF has two regimes:

```text
0x00000000 – 0x7FFFFFFF   standard SUTF-8 framing, 1–6 bytes  (SUTR-1)
0x80000000 – 0xFFFF...FF  0xFE prefix + 8 bytes big-endian    = 9 bytes
0xFF                    reserved, for a future >64-bit domain
```

So the Extended tail uses a **fixed 9-byte escape**, not a variable-length
integer. The design reason is stated in the header: `0xFE` and `0xFF` are
unused as SUTF-8 lead bytes, which are `0xC0`–`0xFD`, so they are free
extension markers. A LEB128 variable-length scheme could not coexist with
SUTF-8 lead bytes, because `0xFE`/`0xFF` would be ambiguous.

`vsutf_codepoint_length()` reproduces the SUTF-8 length table exactly for the
Base range — 1, 2, 3, 4, 5, 6 bytes at the SUTR-1 boundaries — and returns 9
above `0x7FFFFFFF`. `VSUTF_MAX_BYTES` is 9. A codepoint in the inherited trap
range returns 0, so vSUTF cannot carry a trap address.

One consequence is worth noting: because the Base regime is byte-for-byte
SUTF-8, a Base SUCS stream is **already** a valid vSUTF stream. vSUTF is a
strict superset of SUTF-8, and a decoder that understands `0xFE` needs no
special case for Base text.

### 3.3 e-SUST

`e-SUST` is the **page-mapped** Extended transport, built for hypervisor IPC.
Guest systems reference codepoints by coordinate rather than by value, because
a guest and host may not share an address space:

```text
frame = 4 bytes page_index (uint32_t, big-endian)
      + 2 bytes offset     (uint16_t, big-endian)
      = 6 bytes
```

with `ESUST_PAGE_SIZE = 4096` codepoints per page and `ESUST_PAGE_SHIFT = 12`.
A host maps a guest `page_index` to a page-aligned host base address in the
ExtSUCS encoding; the guest's 12-bit `offset` (`ESUST_OFFSET_MASK = 0x0FFF`)
selects within the page. An `esust_mapping_t` holds either a synthetic identity
mapping or a caller-populated table of real ones.

e-SUST is a **TES** in SUCEM terms — a framing over already-encoded data whose
purpose is transport across an isolation boundary, not the representation of
characters. It carries no meaning of its own about what the codepoints mean.

---

## 4. Requirements

1. **Full 64-bit representability.** Every Extended transport MUST represent
   any value from `0` to `0xFFFFFFFFFFFFFFFF`.
2. **Base round-trip.** Base SUTF codepoints MUST round-trip unchanged through
   the Extended transports, at the same numeric value.
3. **No reserved terminal value.** The Extended transports MUST NOT treat
   `0x7FFFFFFF` as a terminator; the plugin space above it is addressable.
   Streams are length-prefixed instead.
4. **Order is the transport's, not the form's.** A transport MUST fix a byte
   order; the SUTF forms it carries MUST remain endian-neutral.
5. **Simple CES.** Transports MUST be simple (no shift/escape state), so that
   a transport is a fixed, table-free function of the code-unit sequence.

---

## 5. Data

| Path | Contents |
|------|----------|
| `Public/0.1.0/transport/` | Transport contract (Base tree) |
| `superunicode_extended/Public/0.1.0/transport/vector/VectorLayout.txt` | Vector unit layout |
| `superunicode_extended/Public/0.1.0/transport/vsutf.txt` | vSUTF variable-length contract |
| `sust/include/sust.h` | SUST master header; SUST-16 |
| `sust/include/sustfixed.h` | Fixed-width family; SUST-N |
| `sust/include/esust.h` | e-SUST page/IPC framing |

Reference implementation: `modules/sust`, exercised by `test_sust_all`.

---

## 6. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| `SUST-16` is canonical big-endian, with LE available | Yes | `sust16.h`; `_be` / `_le` entry points |
| `SUST-16` has no byte-order mark | Yes | Every word `≥ 0x8000` is a marker |
| `encode_*` MUST pair with matching `decode_*` | Yes | `sust16.h` contract |
| Fixed-width transports are big-endian, zero-padded | Yes | `sustfixed.h`; `SUST-256` is 32 bytes |
| `SUST-32` is 4 bytes, `SUST-64` is 8, `SUST-512` is 64 | Yes | `SUST32_BYTES` / `SUST64_BYTES` / `SUST512_BYTES` |
| `SUST-32` covers only 32-bit values; `SUST-64` the full 64 bits | Yes | `sustfixed.h` per-transport range notes |
| `SUST-N` minimum `slot_bytes` is 8 | Yes | `sustn_encode()` contract |
| Full 64-bit representability in Extended transports | Yes | Round-trip `0xFFFFFFFFFFFFFFFF` |
| Base codepoints round-trip unchanged | Yes | Cross-form test |
| Extended transports do not terminate on the Sentinel | Yes | Length-prefixed framing; no `0x7FFFFFFF` check |
| vSUTF Base regime is byte-identical to SUTF-8 | Yes | `vsutf_codepoint_length()` matches SUTR-1 |
| vSUTF Extended regime is `0xFE` + 8 bytes big-endian | Yes | `VSUTF_EXT64_TOTAL` = 9 |
| `0xFE`/`0xFF` are safe SUTF-8 extension markers | Yes | SUTF-8 leads are `0xC0`–`0xFD` |
| `VSUTF_MAX_BYTES` is 9 | Yes | `vsutf.h` |
| e-SUST frame is 6 bytes (4 page + 2 offset) | Yes | `ESUST_IPC_FRAME_BYTES` |
| e-SUST page size is 4096, shift 12 | Yes | `ESUST_PAGE_SIZE`, `ESUST_PAGE_SHIFT` |
| e-SUST fields are big-endian | Yes | `esust.h` layout comment |
| Transports are simple CESs (no escaping) | Yes | Encoder/decoder inspection |
| No byte-order mark in any transport | Yes | Format inspection |
| `SUTF-32` / `SUTF-64` do not exist as forms | Yes | Naming; forms are SUTR-1 |

---

## 7. Terms & Conventions

- **SUST** — SuperUnicode Serialization Transport; the Level 4 CES family.
- **CES** — Character Encoding Scheme; byte order and framing.
- **Transport** — a SUST: a simple, reversible code-units-to-bytes transform.
- **`SUST-16`** — canonical big-endian SUTF-16 serialization; the reference
  transport.
- **`SUST-N`** — the general fixed-width transport; the named widths
  (32/64/128/256/512) are conveniences.
- **Vector** — length-prefixed sequence of 64-bit `sucs_ex_char_t`.
- **vSUTF** — Extended transformation format; SUTF-8 for the Base range,
  `0xFE` + 8 big-endian bytes above it, 9 bytes max. `vsutf.h` is normative.
- **e-SUST** — page-mapped Extended transport: 4096-codepoint pages, 6-byte
  `(page_index, offset)` IPC frames.
- **Simple CES** — a CES with no shift mechanism and no escaping.
- **BANcode** — an SCP trap directive codepoint; transported like any other
  codepoint (SUTR-0 §5).
- **ExtSUCS** — the unbounded 64-bit space; the plugin space sits above the
  Sentinel, which is why Extended framing cannot reuse it as a terminator.

---

**END OF SUTR-4**
