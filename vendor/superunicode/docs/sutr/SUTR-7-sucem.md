# SUTR-7 — SuperUnicode Character Encoding Model (SUCEM)

**Standard ID:** SUTR-7
**Status:** 0.1.0 — Published
**Scope:** The four-level encoding model underlying the whole standard
**Applies To:** every SUTR, SUTS and SUAS document; every implementation
**Last Updated:** 2026-09-30

---

## 1. Overview

This report describes a model for the structure of character encodings. The
SuperUnicode Character Encoding Model (SUCEM) places SuperUnicode in the
context of other character encodings of all types, and of other encoding
models such as the character architecture promoted by the Internet
Architecture Board for use on the internet (RFC 2130), or the Character Data
Representation Architecture (CDRA) defined by IBM for organizing and
cataloging its own proprietary array of character encodings.

Like the Unicode Character Encoding Model, SUCEM separates abstract characters
from physical bits using the same four levels. Each level keeps the standard
names — `ACR`, `CCS`, `CEF`, `CES` — so SuperUnicode encodings can be
described with the same vocabulary as every other encoding on the web.

```text
Level 1   ACR   Abstract Character Repertoire
                the set of characters to be encoded — Unicode, RNUR, SCC, directives

Level 2   CCS   Coded Character Set (SUCS)
                abstract character → 31-bit codepoint, 0x00000000-0x7FFFFFFF

Level 3   CEF   Character Encoding Form (SUTF)
                codepoint → sequence of code units, endian-neutral

Level 4   CES   Character Encoding Scheme & Transport (SUST)
                code units → serialized bytes: order, alignment, framing
```

The critical property of the model is that a **character** is never confused
with a **glyph**, a **code point** is never confused with a **code unit**, and
a **code unit** is never confused with a **byte**. Each level is a distinct
object with distinct invariants, and the SuperUnicode Standard defines each
one exactly once.

---

## 2. Scope

SUTR-7 defines the model and the boundaries between levels. It does not define
the contents of any level: those are SUTR-0 (CCS), SUTR-1 (CEF), SUTR-4 (CES),
and the repertoire registries named in §3. It is the document that explains why
those reports do not overlap.

| Level | SuperUnicode implementation | Report |
|-------|----------------------------|--------|
| ACR | Unicode, RNUR, SCC directives | §3; SUTR-6 for the bridge |
| CCS | Base SUCS, ExtSUCS | SUTR-0, SUTR-5 |
| CEF | SUTF-8/16/4/2, vSUTF | SUTR-1 |
| CES | SUST-16, SUST-32…512/N, e-SUST | SUTR-4 |

---

## 3. Level 1 — Abstract Character Repertoire (ACR)

A character repertoire is an **unordered** set of abstract characters to be
encoded, defined by convention rather than by shape. The SuperUnicode
repertoire is **open**: it is not fixed at a catalogue number, because it is
deliberately designed to grow without ever renumbering what already exists. It
has three sources.

| Source | What it contributes | Registry |
|--------|--------------------|----------|
| **Unicode** | The `0x000000`–`0x10FFFF` repertoire: every assigned Unicode abstract character, at its Unicode numeric value | SUTR-6 |
| **RNUR** | Original scripts, conlangs and neographies, allocated in the Unicode Private Use Areas under a coordinate-pair system `(Set_Number, Code_Point)` | RNUR |
| **SCC** | The System Control Plane: non-printable, in-band kernel directives, style shifts, layout markers and BANcode trap points | SUTR-0 |

### 3.1 The RCCR identity layer

The **RCCR** (Reddit Conlang Code Registry) assigns BCP 47 language tags to
RNUR-encoded conlangs and conscripts, so an abstract character can be
referenced by name across repertoires and mappings.

Its admission rule is deliberately asymmetric, and the asymmetry is the design:

- A **conscript** gets an `art-x-sx-` tag wherever the RNUR holds the
  allocation, unless another authority (e.g. SPUCE) controls it. A writing
  system is a design artefact; even one that exists only to write a real
  language is a distinct, nameable object, and tagging it shadows nothing.
- A **conlang** gets an `art-x-` tag **only** if it is artificial *and* no
  existing BCP 47 primary subtag or ISO 639 code names it. A real language
  never gets an `art-x-` tag, because minting one would create a second,
  competing namespace and make `art-x-hiligaynon` strictly less useful than
  `hil`.

RCCR has **no set layer**, and never will. The RNUR's set layers exist because
the PUA is scarce and contested; the `art-x-` subtree is not scarce, because
the tag namespace itself disambiguates. RNUR eviction moves an allocation to
the *identical* code point in a higher set, so a tag resolves through whatever
set is current — the tag is the stable identity, the coordinate is a runtime
lookup. This is also what lets a tag survive graduation into a real Unicode
block: the tag is retained and re-pointed, which is the strongest argument for
BCP 47 over a bespoke syntax.

### 3.2 Characters are not glyphs

Glyphs are the images that render a character, and the correspondence is not
one-to-one. A single `fi` sequence may render as one ligature or two; an
accented character may be one glyph or several positioned components; a
context-sensitive script substitutes shape, position and width per surrounding
glyph. What is encoded is the character sequence. Rendering it is the host's
business, and SUCEM deliberately places rendering **outside the model**, at or
above Level 1.

### 3.3 Versioning

The Unicode-sourced portion is versioned by the Unicode Standard (SUTR-6 §3).
The RNUR-sourced portion is versioned by its own set layers and the Graduation
Rule. The repertoire only ever grows; nothing is removed, and a new abstract
character never displaces an existing one.

---

## 4. Level 2 — Coded Character Set (CCS)

A coded character set is a mapping from a set of abstract characters to a set
of non-negative integers. SUCS is that mapping: it assigns each abstract
character a 31-bit `sucs_char_t`. The **codespace** is the numerical space it
spans, `0x00000000`–`0x7FFFFFFF`, partitioned into three spaces with different
owners (SUTR-0 §4) and terminated by the Sentinel and trap range (SUTR-0 §6).

The codespace is deliberately non-contiguous in its *use*, not in its span:
the UTF-16 surrogate range `0xD800`–`0xDFFF` is **not** excluded, because
SuperUnicode has no surrogates, so those codepoints are ordinary valid PUA.

**ExtSUCS** is a second, unbounded CCS in the same family, carrying codepoints
in a 64-bit `sucs_ex_char_t` with **zero in-band sentinels** — every function
reports status out of band instead. It inherits the same three-space partition
and trap range, and adds the plugin space above the Sentinel, which mounts
only after the checksum gate (SUTR-5).

---

## 5. Level 3 — Character Encoding Form (CEF)

A character encoding form maps the integers of a CCS to sequences of code
units. A code unit is an integer of a specified binary width. SUTF defines this
level and stops there: **an SUTF is endian-neutral** (SUTR-1).

## 6. Level 4 — Character Encoding Scheme & Transport (CES)

A character encoding scheme is a reversible transformation from sequences of
code units to serialized sequences of bytes. SUST is that level, and it is the
only level in SuperUnicode that knows about byte order, memory layout, register
alignment and frame structure (SUTR-4).

---

## 7. Why the SUTF and SUST Split Is Normative

The repository gives SUTF and SUST separate top-level directories. That
separation is not an organizational convenience; it is the direct consequence
of the Level 3 / Level 4 boundary, and it follows from one question.

> **If the processor byte order changed, would this format change?**
>
> **No** → Level 3, a **CEF**: SUTF-8, SUTF-16, SUTF-4, SUTF-2, vSUTF.
> **Yes** → Level 4, a **CES**: SUST-16, SUST-32/64/128/256/512/N, e-SUST.

SUTF-8 is byte-oriented, so its CES is the identity, which is why there is no
`SUST-8`. SUTF-16 is *not* byte-oriented, so it requires an explicit order
decision, and that decision — not the transformation — is what SUST-16 makes.
The two levels compose as a product: one CEF, many CESs; one CES, many CEFs.

Three consequences, each a rule rather than a preference:

1. **A fixed 32-bit form is not an SUTF form.** Fixed-width framings are
   `SUST-32/64/128/256/512/N`. There is no `SUTF-32`.
2. **Little-endian is not an SUTF form.** The byte order is chosen by
   `SUST-16`, canonically big-endian. There is no `SUTF-16-LE`.
3. **A narrow cast is a serialization.** Reinterpreting a `sucs_char_t` as
   `uint8_t[]` produces a byte array in host order — a Level 4 operation
   performed implicitly. Code needing a portable byte stream MUST call a SUST
   transport explicitly.

Keeping the two in one library would force every consumer to re-derive which
half it was using, and would make "is this artifact portable?" answerable only
by reading the implementation.

---

## 8. Anchoring the System Control Plane

The SCP is where the four levels are easiest to conflate, because SCP
codepoints are simultaneously addresses, directives, and protocol elements.
SUCEM separates the three roles precisely, and that separation is what makes
the kernel damage-control workflow well-defined.

| Level | What the SCP is there |
|-------|-----------------------|
| **1 — ACR** | A set of *non-printable abstract directives*: fatal kernel error, warning, command, soft signal. They are abstract, and they are not glyphs; nothing renders them. |
| **2 — CCS** | A set of *addresses*. The BANcode Registry occupies `0x0011A000`–`0x0011AEFF`, partitioned into B+, W+, C+ and S+ clusters of 2,048, 1,024, 512 and 256 codepoints. A BANcode is purely a number here. |
| **3 — CEF** | A set of *transformed integers*, identical in kind to every other codepoint. An SCP directive is framed by the same SUTF-8 or SUTF-16 rules as a letter. |
| **4 — CES** | A set of *framed packets*. Under e-SUST a BANcode becomes 6 bytes: a 4-byte big-endian page index and a 2-byte offset. Under SUST-16 it becomes a big-endian word. |
### 8.1 The dispatch workflow, level by level

1. **Crash.** The kernel faults and raises a fatal B+ BANcode. At Level 2 this
   is a single address in the B+ cluster; at Level 1 it is the abstract
   directive *fatal kernel error*.
2. **Resolve.** The handler calls `sucs_bancode_to_trap()`, mapping the
   BANcode to its Kernel Security Trap address in `0x7FFFFFF0`–`0x7FFFFFFE`.
   Still Level 2 — a number becomes a different number.
3. **Address.** The trap slot identifies a Damage Control Handler governing a
   cluster of 128 BANcodes. `sucs_trap_to_bancode_range()` inverts the mapping.
4. **Serialize.** Only now does Level 4 enter: the chosen transport frames the
   directive and its payload for the dump destination.

### 8.2 The invariance requirement

A load-bearing consequence: because SCP codepoints are in-band *addresses* at
Level 2, they must survive canonical transformation. SUCF guarantees the SCP,
the trap range and the Sentinel pass through normalization untouched and in
their original stream position, so a kernel directive is never decomposed,
reordered, or dropped by a text pipeline.

The same discipline applies to the Sentinel. Because `0x7FFFFFFF` is an in-band
invalid value, it is a Level 2 fact that constrains Levels 3 and 4: no SUTF
form may be defined to emit it as data, and every SUST transport must be able
to represent it as a terminator. ExtSUCS dissolves this constraint — the space
is unbounded, so `0x7FFFFFFF` becomes an ordinary codepoint and errors move
entirely out of band.

---

## 9. Character Maps and Transfer Encoding Syntax

Two related concepts sit outside the four levels proper, and SuperUnicode has
a direct analogue of each.

- **Character Map (CM)** — a mapping from abstract characters straight to
  serialized bytes, collapsing all four levels into one operation. SuperUnicode
  ships the Unicode bridge as an **identity CM** over
  `0x000000`–`0x10FFFF`, published as
  `Public/0.1.0/mappings/UNICODE.txt`. The `unicode2superunicode` and
  `superunicode2unicode` tools are CM implementations bridging SUCS and UTF-8
  end to end.
- **Transfer Encoding Syntax (TES)** — a reversible transform of encoded data
  that may not contain text at all. **e-SUST** is SuperUnicode's TES: a framing
  over already-encoded data whose purpose is transport across an isolation
  boundary, not the representation of characters. **No compression scheme is
  defined at any level**; compression is deliberately left to the storage layer
  (SUTR-3 §3.2).

---

## 10. Definitions and Acronyms

| Term | Meaning |
|------|---------|
| **ACR** | Abstract Character Repertoire — Level 1 |
| **CCS** | Coded Character Set — Level 2 |
| **CEF** | Character Encoding Form — Level 3 |
| **CES** | Character Encoding Scheme — Level 4 |
| **CM** | Character Map — all four levels in one operation |
| **TES** | Transfer Encoding Syntax |
| **Codespace** | The numerical space spanned by the integers of a CCS |
| **Code unit** | The minimal bit combination representing one unit of encoded text, of a specified width |
| **Sentinel** | `0x7FFFFFFF` — the in-band invalid SUCS codepoint |
| **Code point** | The integer assigned to an abstract character by a CCS |
| **Glyph** | The image that renders a character; not encoded, and not one-to-one with characters |

---

## 11. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| Four levels use the standard ACR/CCS/CEF/CES names | Yes | This document; cross-references in SUTR-0/1/4 |
| Every level defined exactly once across the series | Yes | SUTR-0, SUTR-1, SUTR-4 |
| Character / glyph distinction stated | Yes | §3.2 |
| CEF is endian-neutral; CES fixes order | Yes | SUTR-1 §5; SUTR-4 |
| The byte-order test distinguishes the two levels | Yes | §7 |
| No `SUTF-32` / `SUTF-16-LE` exists as a form | Yes | SUTR-1 §5; SUTR-4 §2.2 |
| SCP mapped to all four levels | Yes | §8 table |
| SCP survives canonical transformation untouched | Yes | SUAS-004 (SUCF) |
| Sentinel constrains Levels 3 and 4 | Yes | SUTR-0 §6; SUTR-1 §1.2; SUTR-4 |
| ExtSUCS removes the in-band sentinel constraint | Yes | §4, §8.2 |
| CM and TES identified as outside the four levels | Yes | §9 |
| No compression defined in the encoding model | Yes | §9; SUTR-3 §3.2 |

---

## 12. Terms & Conventions

- **SUCEM** — SuperUnicode Character Encoding Model; the four-level model.
- **SUCS** — SuperUnicode Character System; the 31-bit Base CCS.
- **ExtSUCS** — the unbounded 64-bit CCS.
- **SUTF / SUST** — the CEF and CES families (SUTR-1, SUTR-4).
- **ACR** — Abstract Character Repertoire.
- **RNUR** — Reddit Neographical Unicode Registry; PUA coordinate system.
- **RCCR** — Reddit Conlang Code Registry; BCP 47 tags, no set layer.
- **SPUCE** — the authority whose sectors (Enochian, Litterae Ignotae, Theban)
  are excluded from RCCR by its admission rule.
- **SCP** — System Control Plane.
- **BANcode / WARNcode / COMcode / SOFTcode** — the four SCP severity clusters.
- **Sentinel** — `0x7FFFFFFF`.
- **UCS / UAX / UTR** — Unicode Character Standard / Unicode Annex / Unicode
  Technical Report; SUTR plays the role of UTR in this standard.
- **RFC 2130** — IAB Character Architecture.
- **CDRA** — IBM Character Data Representation Architecture.

---

**END OF SUTR-7**
