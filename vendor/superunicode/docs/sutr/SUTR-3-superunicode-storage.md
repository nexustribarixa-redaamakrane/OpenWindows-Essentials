# SUTR-3 — SuperUnicode Storage

**Standard ID:** SUTR-3
**Status:** 0.1.0 — Published
**Scope:** Partition formats and the mount policy for SuperUnicode media
**Applies To:** removable media, boot and rescue payloads, plugin partitions
**Last Updated:** 2026-09-30

---

## 1. Overview

Storage is governed by the OpenWindows Storage specification
(`compat/OpenWindows-storage/`, `libowfs.a` = OWFS, `libusfs.a` = USFS). This
report records the SuperUnicode-specific policy layered on top of those two
filesystems.

A **SuperUnicode Partition** is a removable filesystem whose layout is declared
by **SuperUnicode machine instructions** — that is, by SCP codepoints (SUTR-0
§5). The layout is not described out of band, so a partition's structure is
expressed in the same in-band directive vocabulary the rest of the system uses.

### 1.1 Goals

- **Portability where it is safe.** A bugfix or rescue payload must survive
  being carried on whatever media an operator has.
- **Integrity before availability.** A payload that fails its checksum is
  rejected rather than repaired; there is no repair path in the base policy.
- **No writable SuperUnicode mounts.** Nothing in the standard mounts
  read-write, so a compromised payload cannot persist.
- **Plugin isolation.** Plugin partitions are an Extended-only feature with
  stricter rules than Base payloads, because they extend the codepoint space.

---

## 2. Formats

| Format | Library | Description |
|--------|---------|-------------|
| **OWFS** | `libowfs.a` | Native drive filesystem. Data starts at drive offset `0x10000`. Integrity via **CRC32c + Fletcher-64**. Identity via `htl_device_t` + `ow_sec`. Optional **ChaCha20** data-at-rest encryption. |
| **USFS** | `libusfs.a` | Portable external-media filesystem. |

Both integrity algorithms are computed over the partition payload, and both
are used together deliberately: CRC32c is a hardware-accelerated polynomial
check good at detecting burst corruption, while Fletcher-64 is a cheaper
running sum that catches localized damage across longer spans. Neither alone
covers the other's failure modes adequately for media that is expected to fail
slowly.

OWFS is the *native* format and is the only one permitted for plugins, because
the plugin trust chain (SUTR-5) depends on OWFS identity and integrity
guarantees that USFS does not provide.

---

## 3. Policy

The normative policy, as published in `partitions/spec.txt`:

| Partition class | Formats permitted | Purpose |
|-----------------|-------------------|---------|
| **Base SuperUnicode Partition** | OWFS **or** USFS | Bugfix and rescue payloads **only** |
| **Plugin Partition** | **OWFS only** (never USFS) | Extended-only; carries plugin blobs |

Three rules, stated as MUSTs:

1. Base SuperUnicode uses SuperUnicode Partitions **only** for bugfix and
   rescue payloads. It is not a general storage mechanism, and the absence of
   compression or garbage collection is deliberate.
2. Plugin Partitions are an **Extended-only** feature and MUST be OWFS, never
   USFS.
3. All partitions mount **read-only**.

### 3.1 The format restriction

The OWFS-only rule for plugins is a trust decision, not a portability one. A
plugin blob is verified by checksum (SUTR-5) and then contributes codepoint
ranges to the live address space. Binding that to a filesystem that carries
`htl_device_t` identity and dual-integrity checking means a mounted plugin
partition has a verifiable provenance chain. USFS, being designed for
removable external media, offers neither.

### 3.2 No compression

The standard defines **no compression scheme at any level** (SUTR-7, Transfer
Encoding Syntax). Compression is deliberately left to the storage layer and
out of the encoding model, so that the encoding of a codepoint never depends
on the medium carrying it. An implementation may compress a partition; the
SuperUnicode stream inside it is unchanged.

---

## 4. Mount Policy

```text
read-only always
Base SuperUnicode mounts its partition at boot
plugin partitions mount only after the boot checksum gate passes
```

The ordering is the security property. A plugin partition MUST NOT be mounted
during ordinary boot; it is mounted only after the boot checksum gate has
validated staged blobs (SUTR-5 §4). Until that gate passes, no plugin
contributes codepoints to ExtSUCS, so a corrupt or tampered blob has no
window in which its ranges are addressable.

The mode interaction is covered by SUTR-1 §6: plugin partitions exist only in
`SUCS_MODE_EXTENDED`, and switching to that mode requires a kernel restart.

---

## 5. Data

| Path | Contents |
|------|----------|
| `Public/0.1.0/partitions/spec.txt` | The normative partition policy, published in both /Public trees |
| `superunicode/Public/zipped/partitions.zip` | Packaged distribution |
| `compat/OpenWindows-storage/owfs/` | OWFS headers, incl. `owfs_types.h` |
| `compat/OpenWindows-storage/usfs/` | USFS headers, incl. `usfs_types.h` |

The policy text is duplicated verbatim in both the Base and Extended
`/Public` trees, so a reader of either tree finds the same contract.

---

## 6. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| Partition layout declared by SCP instructions | Yes | `partitions/spec.txt` |
| OWFS data starts at offset `0x10000` | Yes | `owfs_types.h` |
| Integrity is CRC32c **and** Fletcher-64 | Yes | Both computed and verified |
| ChaCha20 at-rest encryption optional | Yes | Feature-detect, not required |
| Base partitions MAY be OWFS or USFS | Yes | Mount accepts either |
| Base partitions used only for bugfix/rescue | Yes | Policy; no general mounts |
| Plugin partitions MUST be OWFS | Yes | Reject USFS with a diagnostic |
| All partitions mount read-only | Yes | No write path exists |
| Plugin partitions mount only after the checksum gate | Yes | Mount ordering in boot |
| Plugin partitions are Extended-only | Yes | `SUCS_MODE_EXTENDED` required |
| No compression defined in the encoding model | Yes | SUTR-7 §Transfer Encoding Syntax |

---

## 7. Terms & Conventions

- **SuperUnicode Partition** — a removable filesystem whose layout is declared
  by SuperUnicode machine instructions.
- **OWFS** — native drive filesystem; `libowfs.a`; `0x10000` data offset;
  CRC32c + Fletcher-64; `htl_device_t` + `ow_sec` identity; optional ChaCha20.
- **USFS** — portable external-media filesystem; `libusfs.a`.
- **CRC32c** — Castagnoli polynomial checksum, hardware-accelerated.
- **Fletcher-64** — running-sum checksum over longer spans.
- **ChaCha20** — optional stream cipher for data at rest.
- **Base SuperUnicode Partition** — OWFS or USFS, bugfix and rescue payloads.
- **Plugin Partition** — OWFS, read-only, Extended-only, post-checksum-gate.
- **SCP** — System Control Plane; the directive vocabulary that declares
  partition layout (SUTR-0 §5).
- **ExtSUCS** — the unbounded 64-bit space plugins extend (SUTR-5).
- **Boot checksum gate** — the validation stage a plugin blob must pass before
  mount (SUTR-5 §4).

---

**END OF SUTR-3**
