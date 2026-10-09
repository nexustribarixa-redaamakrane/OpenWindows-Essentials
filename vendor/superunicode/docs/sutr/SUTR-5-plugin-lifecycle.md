# SUTR-5 — Plugin Lifecycle & Blob Format

**Standard ID:** SUTR-5
**Status:** 0.1.0 — Published
**Scope:** ExtSUCS plugin space — blob format, validation gates, mount, registry
**Applies To:** plugin authors, `plugin_pack` / `plugin_verify`, the boot checksum gate
**Last Updated:** 2026-09-30

---

## 1. Overview

SUTR-5 defines how a plugin goes from a packed blob on a partition to
registered codepoints in the live ExtSUCS address space. It specifies the blob
format, the four validation gates, the status codes, and the mount ordering
that together make the plugin space safe to expose to the address space.

The plugin space is the one part of SuperUnicode that is **not** frozen at
release. Base SUCS is 31 bits and permanent; ExtSUCS is unbounded and accepts
new codepoint ranges from plugins. That openness is the whole point of the
design, and it is also the only part of the system that can extend the meaning
of a codepoint — which is why the gate sequence here is normative and strict.

---

## 2. Blob Format

A plugin blob is one packed file:

```text
header (94 bytes) | range_count × 16-byte ranges | payload bytes
```

### 2.1 Header Layout

`sucs_plugin_blob_header_t`, packed:

| Field | Type | Bytes | Notes |
|-------|------|-------|-------|
| `magic` | `uint32_t` | 4 | `SUCS_PLUGIN_BLOB_MAGIC` = `0x53435343` (`"SUCS"`) |
| `blob_version` | `uint16_t` | 2 | Blob format version |
| `ver_major` | `uint8_t` | 1 | Plugin semantic version |
| `ver_minor` | `uint8_t` | 1 | |
| `ver_patch` | `uint8_t` | 1 | |
| `reserved` | `uint8_t` | 1 | Must be zero |
| `id` | `char[64]` | 64 | `SUCS_PLUGIN_ID_MAX`; NUL-terminated |
| `range_count` | `uint32_t` | 4 | Number of 16-byte range records following |
| `blob_size` | `uint32_t` | 4 | Total blob size in bytes |
| `crc32c` | `uint32_t` | 4 | CRC32c (Castagnoli) |
| `fletcher64` | `uint64_t` | 8 | Fletcher-64 |

Total: 4 + 2 + 1 + 1 + 1 + 1 + 64 + 4 + 4 + 4 + 8 = **94 bytes**.

Range records are 16-byte little-endian pairs (`start`, `end`) — 8 bytes each
for the 64-bit ExtSUCS domain. The payload follows the ranges.

### 2.2 Checksum Computation

`crc32c` and `fletcher64` are computed over the **entire blob** with the two
checksum fields themselves **zeroed**. Computing over the stored value instead
would be self-referential and unsatisfiable.

The dual algorithm is the same choice as SUTR-3: CRC32c is hardware-accelerated
and good at burst corruption; Fletcher-64 is a cheaper running sum that catches
damage spread across long spans. Both MUST be present, and both MUST match.

---

## 3. Status Codes

The plugin ABI exposes a 14-value status enum, `sucs_plugin_status_t`:

| Code | Value | Meaning |
|------|-------|---------|
| `SUCS_PLUGIN_OK` | 0 | Operation succeeded |
| `SUCS_PLUGIN_ERR_INVALID_BLOB` | 1 | Malformed plugin blob |
| `SUCS_PLUGIN_ERR_INVALID_ID` | 2 | Empty / malformed plugin id |
| `SUCS_PLUGIN_ERR_INVALID_RANGE` | 3 | Malformed codepoint range |
| `SUCS_PLUGIN_ERR_RANGE_BELOW_BASE` | 4 | Range does not extend past base limit |
| `SUCS_PLUGIN_ERR_RANGE_COLLISION` | 5 | Range overlaps an active plugin |
| `SUCS_PLUGIN_ERR_CHECKSUM_MISMATCH` | 6 | Stored checksum does not match blob |
| `SUCS_PLUGIN_ERR_NOT_OWFS` | 7 | Plugin partition must be OWFS |
| `SUCS_PLUGIN_ERR_MOUNT_FAILED` | 8 | Partition mount failed |
| `SUCS_PLUGIN_ERR_BUFFER_TOO_SMALL` | 9 | Blob exceeds staging capacity |
| `SUCS_PLUGIN_ERR_DUPLICATE_ID` | 10 | Plugin id already staged / active |
| `SUCS_PLUGIN_ERR_STAGING_FULL` | 11 | Staging table is full |
| `SUCS_PLUGIN_ERR_UNSUPPORTED_VERSION` | 12 | Blob format version unsupported |
| `SUCS_PLUGIN_REBOOT_REQUIRED` | 13 | Plugin staged; system restart required |

Note that 13 is a **success** condition, not an error: staging a plugin always
requires a restart, and the distinct code makes that unambiguous to a caller
that would otherwise have to infer it from a generic `OK`. Gate failures return
the specific diagnostic state rather than a collapsed error, so an operator
can tell a bad checksum from a colliding range without re-deriving it.

---

## 4. The Gate Sequence

```text
blob
 ├─► checksum gate   CRC32c + Fletcher-64 over the whole blob (fields zeroed)
 ├─► range gate      ranges sorted, non-overlapping, strictly above the base limit
 ├─► collision gate  no overlap with any mounted plugin's ranges
 ├─► mount gate      OWFS only, read-only
 └─► register        ranges enter ExtSUCS and lookups resolve
```

**1. Checksum gate.** Both checksums must match. This runs first because it is
the cheapest way to reject a corrupt or tampered blob, before any structural
parsing of ranges.

**2. Range gate.** Each range must be well-formed (`start` ≤ `end`), the range
list must be sorted, ranges must not overlap *each other*, and every range must
lie **strictly above the base limit** — the plugin space begins above
`0x7FFFFFFF`. A blob cannot claim part of the frozen Base space, which is what
keeps Base SUCS permanent (SUTR-0 §6, SUTR-6).

**3. Collision gate.** No range may overlap the ranges of any already-mounted
plugin. This is what makes the plugin space *additive* and non-overlapping: two
plugins can coexist, and their ranges can never be ambiguous, because a codepoint
address resolves to at most one plugin.

**4. Mount gate.** The partition must be OWFS and mounts read-only (SUTR-3 §3).
USFS is rejected with `SUCS_PLUGIN_ERR_NOT_OWFS`; there is no fallback.

**5. Register.** Only after all four gates pass do the ranges enter ExtSUCS and
ordinary lookups resolve them. Registration happens during the early boot
commit, never at runtime.

### 4.1 Reboot Requirement

Staging returns `SUCS_PLUGIN_REBOOT_REQUIRED` (13) on success. The plugin is
*staged*, not active: its ranges do not resolve until the kernel restart
commits the staged table. This mirrors the mode-switch contract in SUTR-1 §6 —
`sucs_request_mode_switch()` stages, `sucs_commit_mode_on_boot()` commits —
and it is why no in-band path exists to activate a plugin while text is in
flight. A codepoint that becomes valid mid-stream would break the invariant
that a stream's meaning is fixed by its framing.

---

## 5. Partition Requirements

| Requirement | Value | Rationale |
|-------------|-------|-----------|
| Format | **OWFS only** | Trust chain: OWFS carries `htl_device_t` identity and dual integrity (SUTR-3) |
| Mount mode | read-only | No persistence for a tampered payload |
| Mode | `SUCS_MODE_EXTENDED` | Plugin space exists only in the 64-bit Extended domain |
| Mount point | after boot checksum gate | No window in which unvalidated ranges are addressable |

Per SUTR-3 §4, a plugin partition MUST NOT mount during ordinary boot. Until
the checksum gate passes, no plugin contributes to ExtSUCS.

---

## 6. Tooling

| Tool | Purpose |
|------|---------|
| `plugin_pack` | Builds a blob from ranges and a payload, computing both checksums |
| `plugin_verify` | Re-runs the checksum and range gates offline, against a blob |

Both reproduce the gate sequence without a mounted partition, so a plugin
author validates a blob before shipping it rather than discovering the problem
at boot. `sample_plugin.scsp` is a worked example.

---

## 7. Data and Implementation

| Path | Contents |
|------|----------|
| `superunicode_extended/plugin/include/superunicode_extended/plugin.h` | Blob header, magic, `SUCS_PLUGIN_ID_MAX`, status enum |
| `.../plugin_checksum.h` | CRC32c + Fletcher-64 |
| `.../plugin_stage.h` | Staging; returns `SUCS_PLUGIN_REBOOT_REQUIRED` |
| `.../plugin_partition.h` | OWFS mount and `plugin_id` association |
| `.../plugin_boot.h` | Boot-time commit |
| `superunicode_extended/plugin/tools/` | `plugin_pack.c`, `plugin_verify.c` |
| `superunicode_extended/Public/zipped/Plugin-SDK.zip` | Distributed SDK |
| `superunicode_extended/Public/0.1.0/extsucd/` | ExtSUCD registry (referenced by the boot gate) |

Conformance: `test_plugin_lifecycle`.

---

## 8. Compliance Matrix

| Requirement | Mandatory | Verification |
|-------------|-----------|--------------|
| Blob header is exactly 94 bytes, packed | Yes | `sizeof(sucs_plugin_blob_header_t)` |
| Magic is `0x53435343` (`"SUCS"`) | Yes | `SUCS_PLUGIN_BLOB_MAGIC` |
| `id` is NUL-terminated, max 64 bytes | Yes | `SUCS_PLUGIN_ID_MAX` |
| Range records are 16-byte little-endian pairs | Yes | Blob layout |
| `crc32c` and `fletcher64` computed over the whole blob, fields zeroed | Yes | `plugin_checksum.h`; reference recomputation |
| Checksum gate runs before range parsing | Yes | Gate order |
| Ranges sorted, non-overlapping, strictly above base limit | Yes | `SUCS_PLUGIN_ERR_INVALID_RANGE` / `_RANGE_BELOW_BASE` |
| No overlap with any mounted plugin | Yes | `SUCS_PLUGIN_ERR_RANGE_COLLISION` |
| Plugin partition MUST be OWFS | Yes | `SUCS_PLUGIN_ERR_NOT_OWFS` |
| Mounted read-only | Yes | `plugin_partition.h` |
| Staging returns `SUCS_PLUGIN_REBOOT_REQUIRED` (13) | Yes | `sucs_plugin_status_t` |
| Registration only at boot commit | Yes | `plugin_boot.h` |
| Plugin space is Extended-only | Yes | `SUCS_MODE_EXTENDED` required |
| `plugin_verify` reproduces gates offline | Yes | `plugin_verify.c` |

---

## 9. Terms & Conventions

- **Plugin** — an ExtSUCS extension contributing codepoint ranges to the space
  above the Sentinel.
- **Blob** — the packed on-disk/slot plugin file: 94-byte header, range
  records, payload.
- **`SUCS_PLUGIN_BLOB_MAGIC`** — `0x53435343`, the ASCII bytes `SUCS`.
- **`SUCS_PLUGIN_ID_MAX`** — 64; the maximum plugin identifier length.
- **Checksum gate** — CRC32c + Fletcher-64 over the whole blob.
- **Range gate** — structural validity, sortedness, non-overlap, and
  above-base-limit.
- **Collision gate** — non-overlap with already-mounted plugins.
- **Mount gate** — OWFS-only, read-only.
- **Staging** — accepting a validated blob as pending; returns
  `SUCS_PLUGIN_REBOOT_REQUIRED`.
- **Commit** — activating staged plugins during early boot.
- **Base limit** — `0x7FFFFFFF`, the Sentinel; plugin ranges lie strictly
  above it (SUTR-0 §6).
- **ExtSUCS** — the unbounded 64-bit space that plugins extend.
- **ExtSUCD** — the ExtSUCS registry consulted at the boot gate.
- **TES** — Transfer Encoding Syntax; e-SUST (SUTR-4) is the transport-layer
  analogue of a TES, and plugin blobs are framed data, not text.

---

**END OF SUTR-5**
