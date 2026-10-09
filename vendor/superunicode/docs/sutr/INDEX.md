# SUTR — SuperUnicode Technical Reports

> Architecture Whitepapers & Foundations

This directory contains technical reports documenting architectural
rationale, design explorations, comparative analyses, and foundational
research supporting the SuperUnicode standard.

The SUTR series plays the role of Unicode's UAX/UTR documents: each report
formalizes one contract of the standard, and each is tracked per release.

## Document Index

| ID | Title | Status | Source |
|----|-------|--------|--------|
| [SUTR-0](SUTR-0-sucs-core.md) ([web](https://nexustribarixa-redaamakrane.github.io/superunicode/reports/SUTR-0.html)) | SUCS Core — hierarchy, three-space layout, SCP, Traps, Sentinel | 0.1.0 | Repo |
| [SUTR-1](SUTR-1-sutf.md) ([web](https://nexustribarixa-redaamakrane.github.io/superunicode/reports/SUTR-1.html)) | SUTF — the Character Encoding Forms: SUTF-8/16/4/2 and vSUTF | 0.1.0 | Repo |
| [SUTR-2](SUTR-2-suca.md) ([web](https://nexustribarixa-redaamakrane.github.io/superunicode/reports/SUTR-2.html)) | SUCA — SuperUnicode Collation Algorithm | 0.1.0 | Repo |
| [SUTR-3](SUTR-3-superunicode-storage.md) ([web](https://nexustribarixa-redaamakrane.github.io/superunicode/reports/SUTR-3.html)) | SuperUnicode Storage — the OWFS/USFS partition policy | 0.1.0 | Repo |
| [SUTR-4](SUTR-4-extsucs-transport.md) ([web](https://nexustribarixa-redaamakrane.github.io/superunicode/reports/SUTR-4.html)) | ExtSUCS Transport — vector, vSUTF and e-SUST framing | 0.1.0 | Repo |
| [SUTR-5](SUTR-5-plugin-lifecycle.md) ([web](https://nexustribarixa-redaamakrane.github.io/superunicode/reports/SUTR-5.html)) | Plugin Lifecycle & Blob Format — stage, checksum gate, mount, registry | 0.1.0 | Repo |
| [SUTR-6](SUTR-6-unicode-bridge.md) ([web](https://nexustribarixa-redaamakrane.github.io/superunicode/reports/SUTR-6.html)) | Unicode Compatibility Bridge — the permanent 1:1 guarantee | 0.1.0 | Repo |
| [SUTR-7](SUTR-7-sucem.md) ([web](https://nexustribarixa-redaamakrane.github.io/superunicode/reports/SUTR-7.html)) | SUCEM — the SuperUnicode Character Encoding Model: ACR → SUCS → SUTF → SUST | 0.1.0 | Repo |

## Core invariants every report honors

- `SUCS_CP` is 31-bit; `sucs_ex_char_t` is 64-bit.
- `0x000000–0x10FFFF` is permanently 1:1 with Unicode.
- The 16 terminal values `0x7FFFFFF0`–`0x7FFFFFFF` are unallocatable: 15
  trap slots, then the Sentinel at `0x7FFFFFFF`. The last allocatable
  codepoint is `0x7FFFFFEF`.
- Plugin codepoints start strictly above `0x7FFFFFFF` and mount only after
  the checksum gate.

## SUCEM model

SUTR-7 defines SUCEM, the four-level model that the rest of the series
specializes:

```text
Level 1   ACR   Abstract Character Repertoire   — Unicode, RNUR, SCC directives
Level 2   CCS   Coded Character Set (SUCS)      — 31-bit, 0x00000000–0x7FFFFFFF
Level 3   CEF   Character Encoding Form (SUTF)  — endian-neutral code-unit sequences
Level 4   CES   Character Encoding Scheme (SUST) — byte order, alignment, framing
```

SUTF and SUST are separate modules because they answer different questions:
*if the processor byte order changed, would this format change?* No → CEF
(SUTF). Yes → CES (SUST). A fixed 32-bit form is `SUST-32`, not an SUTF form.

## Related families

- [SUAS — Architecture Standards](../suas/INDEX.md) — core kernel invariants
  and conformance rules (SUAS-001…004, ratified).
- [SUTS — Technical Specifications](../suts/INDEX.md) — modular driver and
  extension specifications (SUTS-001, ratified).

## Status

**SUTR-0 through SUTR-7 are published at 0.1.0, and all eight have markdown
sources in this directory**, in parity with SUAS and SUTS. The *web* column links
to the published page, which is now **generated from the document beside it** by
`website/pages/sutr.ps1`.

**The repository is authoritative, and the site is a build artefact of it.** The
generator reads each specification and emits the page, so the two cannot drift:
a change to a file here appears on the site at the next `website/dispatch.ps1`
run. Every generated page carries a provenance note naming its source file and
recording that the repository copy governs.

The previously hand-authored SUTR pages in `website/pages/reports.ps1` have been
deleted, so this directory is the only place the SUTR page content is written.
`docs/` itself is not deployed, so a relative `.md` link between specifications
is rewritten by the generator onto the published page that carries the same
content; an unmapped one degrades to plain text rather than shipping a 404.

The two website pages that used to disagree with the headers now agree, because
they were corrected and the SUTR page is generated. `standard/hierarchy.html`
previously stated a seven-level chain (`block`/`range` as address fields) that
`sucs_plane.h` does not define — it defines four bit-fields, Zone 7 / District 8
/ Plane 8 / Offset 8, with Block and Range as variable-length *semantic*
groupings. The SUTR-4 page previously described vSUTF as LEB128-style, where
`vsutf.h` specifies SUTF-8 for the Base range plus a fixed `0xFE` + 8-byte
escape above it. Both are recorded in SUTR-0 and SUTR-4 here, and both now reach
the site.
