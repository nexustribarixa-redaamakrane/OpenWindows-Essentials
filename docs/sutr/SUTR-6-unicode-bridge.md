# SUTR-6 — Unicode Compatibility Bridge

**Standard ID:** SUTR-6
**Status:** 0.1.0 — Published
**Scope:** The permanent 1:1 guarantee across `0x000000`–`0x10FFFF`
**Applies To:** every SuperUnicode consumer; text interchange with Unicode systems
**Last Updated:** 2026-09-30

---

## 1. Overview

Codepoints `0x000000`–`0x10FFFF` correspond **1:1** with Unicode codepoints of
the same numeric value. This is the *Unicode Compatibility Space*, and the
correspondence is the single most load-bearing promise SuperUnicode makes to
the outside world.

```text
SUCS 0x000000 ↔ U+000000   // bi-directional, lossless
SUCS 0x10FFFF ↔ U+10FFFF   // the ceiling of the bridge
```

### 1.1 Why it matters

A conlang or neography that has graduated into the Unicode standard is already
allocated a codepoint. SuperUnicode's promise is that the same abstract
character occupies the same numeric value in both systems, forever — so
graduation is a *bookkeeping* event (a new tag is issued, an allocation is
recorded) and never a re-encoding. A conlang encoded in the bridge today will
not need a migration when it is encoded natively tomorrow. This is the
strongest argument for the design, and it is why the guarantee is stated as
permanent rather than as a version-to-version compatibility claim.

---

## 2. The Guarantee

Three properties, all normative:

**1. Bidirectional and lossless.** `sucs_downcast()` of a bridged codepoint
yields the identical Unicode value, and every Unicode codepoint has a
SuperUnicode identity. No information is added, removed, or transformed at the
boundary.

**2. Permanent by position, not by version.** The mapping is fixed at release
0.1.0 and does **not** change when Unicode publishes new versions, new blocks,
or new codepoints. This is the crucial distinction: *compatibility* is a weaker
claim than *identity*, and SuperUnicode claims identity.

**3. Growth without remapping.** New Unicode codepoints appear at the same
numeric values they occupy in Unicode. The bridge tracks Unicode growth with no
translation table, no surrogate escape, and no re-encoding of existing text.

The bridge stops exactly where the System Control Plane begins: above
`0x10FFFF`, SuperUnicode is fully its own system (SUTR-0 §4).

---

## 3. The Mapping Is Frozen; the Data Is Not

This is the distinction most often collapsed, and it is worth stating plainly:

> The **mapping** is frozen by position. The **data** published about the
> bridge — block definitions and character names — is regenerated from a
> concrete Unicode release whenever one is adopted.

So `0x1F600` is `U+1F600 GRINNING FACE` in SuperUnicode 0.1.0 and in every
future release, but the *table* that says so was built from a specific UCD
snapshot. Adopting Unicode 19 would change the tables, not the mapping.

The current synchronized release is queryable at compile time:

| Macro | Value | Meaning |
|-------|-------|---------|
| `SUCS_UNICODE_VERSION_MAJOR` / `_MINOR` | `18` / `0` | Unicode release the UCS/UCB data is synchronized with |
| `SUCS_UCD_UNICODE_VERSION` | `1800` | Same, packed as `major * 100 + minor` |
| `SUCS_UCD_BLOCK_COUNT` | `353` | Unicode blocks in `sucs_compat.h` |
| `SUCS_UCD_NAME_COUNT` | `41232` | Named codepoints in `sucs_ucd_names.h` |

`SUCS_UNICODE_MAX_COMPAT` fixes the bridge ceiling at `0x0010FFFFUL`, and is
independent of the synchronized release: it does not move when Unicode grows,
because the bridge ceiling is a SuperUnicode constant, not a Unicode one.

---

## 4. Adopting a New Unicode Release

The tables are regenerated from `unicode.org/Public/<version>/ucd/` by
`superunicode/tools/gen_ucd_names.py`, which carries the target version in
`SUCD_UNICODE_VERSION` and resolves the download and output paths from it.
Adopting a release means updating that constant, regenerating
`sucs_compat.h`, `sucs_ucd_names.h` and `sucs_ucd_names.c`, and updating the
pinned expectations in the test suite.

### 4.1 Test-Pinned Values

`test_sucs_ucd` pins the version, block count and name count, and asserts
block sortedness, non-overlap, and the Unicode 18 additions. A **partial**
bump — new macro without regenerated tables, or vice versa — therefore fails
the build rather than shipping inconsistent data. The suite also pins the
practical edge cases of the generated name table:

- The `First`/`Last` range markers of a block (`<CJK Ideograph, First>`) carry
  **no** name, so `0x4E00` and `0x9FFF` look up as unnamed even though
  `0x4E01`–`0x9FFE` are named.
- Unassigned codepoints look up as unnamed; `0x18E00` is unassigned, so the
  Jurchen block is probed at its first assigned codepoint, `0x191A0`.
- The packed name pool stores names **without** a NUL terminator, and
  `sucs_ucd_get_name()` returns a non-NUL-terminated slice. Callers compare
  with `memcmp` over `sucs_ucd_name_length()` bytes; the
  `sucs_ucd_get_name_copy()` variant is the one that NUL-terminates.

---

## 5. ExtSUCS Shares This Data Set

`superunicode_extended` keeps **no UCD tables of its own**:

- `extsucs_compat.h` re-exports the base block table and adds the aliases
  `EXTSUCS_UNICODE_VERSION_MAJOR`, `EXTSUCS_UNICODE_VERSION_MINOR`,
  `EXTSUCS_UCD_UNICODE_VERSION` and `EXTSUCS_UCD_BLOCK_COUNT`.
- `extsucs_ucd_names.h` re-exports the base name API and adds
  `EXTSUCS_UCD_NAME_COUNT`.
- `extsucs_ucd_names.c` delegates name lookups to the base implementation, and
  `extsucs_ucd_block_lookup()` / `extsucs_is_in_ucd_block()` wrap the base
  block table.

So both modules always report the same Unicode release, by construction rather
than by convention. `test_extsucs_ucd` asserts that the ExtSUCS aliases equal
the base macros, so a **one-sided** bump — updating the base tables without
extending the aliases — also fails the build. The ExtSUCS test additionally
checks that a 64-bit value above the bridge ceiling is rejected rather than
silently truncated to 31 bits.

This delegation is a deliberate consequence of §2: if the two modules held
independent UCD snapshots, they could disagree about what `0x1F600` is, and a
document that says "1:1 with Unicode" would mean two different things depending
on which library answered.

---

## 6. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| `0x000000`–`0x10FFFF` is 1:1 with Unicode, both directions | Yes | `sucs_downcast()`; `test_sucs_ucd` |
| Mapping is permanent; new Unicode uses the same values | Yes | `SUCS_UNICODE_MAX_COMPAT` = `0x0010FFFFUL` |
| Bridge ceiling does not move with Unicode releases | Yes | Constant independent of `SUCD_UNICODE_VERSION` |
| Version, block count and name count queryable at compile time | Yes | Four macros in `sucs_compat.h` / `sucs_ucd_names.h` |
| Blocks sorted and non-overlapping | Yes | `test_sucs_ucd` |
| `First`/`Last` range markers carry no name | Yes | `0x4E00`, `0x9FFF` unnamed |
| Unassigned codepoints carry no name | Yes | `0x18E00` unnamed; `0x191A0` named |
| Packed pool is not NUL-terminated | Yes | `memcmp` + `sucs_ucd_name_length()` |
| `sucs_ucd_get_name_copy()` NUL-terminates | Yes | Copy variant test |
| Partial Unicode bump fails the build | Yes | `test_sucs_ucd` pinned values |
| ExtSUCS holds no independent UCD tables | Yes | `extsucs_ucd_names.c` delegates |
| ExtSUCS aliases match base macros | Yes | `test_extsucs_ucd` |
| One-sided bump fails the build | Yes | `test_extsucs_ucd` |
| Above-bridge 64-bit values rejected, not truncated | Yes | `test_extsucs_ucd` 64-bit guards |
| Regeneration is scripted from the UCD | Yes | `gen_ucd_names.py`, `SUCD_UNICODE_VERSION` |

---

## 7. Terms & Conventions

- **Unicode Compatibility Bridge** — the 1:1 correspondence between
  `0x000000`–`0x10FFFF` and Unicode codepoints of the same value.
- **Unicode Compatibility Space** — the bridged region itself.
- **`SUCS_UNICODE_MAX_COMPAT`** — the bridge ceiling, `0x0010FFFFUL`.
- **UCD** — Unicode Character Database; the source of block and name data.
- **UCS** — Unicode Character Standard; the abstract repertoire.
- **UCB** — Unicode Code Block; the block table in `sucs_compat.h`.
- **`sucs_downcast()`** — the bidirectional bridge accessor.
- **Packed name pool** — the NUL-less concatenated name storage in
  `sucs_ucd_names.c`.
- **Range marker** — a `<Block, First>`/`<Block, Last>` pseudo-name, which is
  skipped by the name generator.
- **Graduation** — a conlang or script moving from the Native space into the
  bridge; a bookkeeping event under this guarantee, never a re-encoding.
- **RNUR** — Reddit Neographical Unicode Registry; the Native-space
  coordinate system, outside the bridge by construction.
- **SUTR-0** — SUCS Core, which defines the bridge as one of three spaces.

---

**END OF SUTR-6**
