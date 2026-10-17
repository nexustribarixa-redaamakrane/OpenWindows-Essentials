/*
 * acpitbl.c - OpenWindows ACPI Table Parser (.owc)
 *
 * Walks the RSDT/XSDT indirection described by the ACPI specification.
 * All addresses are treated as physical (identity-mapped) by the caller.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "acpitbl.h"

/* ── Internal state ───────────────────────────────────────────── */

static acpitbl_info_t acpitbl_state;
static const acpitbl_header_t *acpitbl_root;
static bool acpitbl_initialized;

/* ── Helpers ──────────────────────────────────────────────────── */

/* Compare two fixed-length byte strings without libc. */
static bool acpitbl_sig_eq(const char *a, const char *b, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (a[i] != b[i]) return false;
    }
    return true;
}

/* Little-endian loads from unaligned table records. */
static uint32_t acpitbl_load_u32(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t acpitbl_load_u64(const uint8_t *p)
{
    return (uint64_t)acpitbl_load_u32(p) |
           ((uint64_t)acpitbl_load_u32(p + 4) << 32);
}

static uint8_t acpitbl_sum(const uint8_t *data, size_t length)
{
    uint8_t sum = 0u;
    for (size_t i = 0; i < length; ++i) {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

/* ── Implementation ───────────────────────────────────────────── */

bool acpitbl_checksum_ok(const void *data, size_t length)
{
    if (!data || length == 0u) return false;
    return acpitbl_sum((const uint8_t *)data, length) == 0u;
}

bool acpitbl_validate_rsdp(const void *rsdp_address, acpitbl_info_t *info)
{
    if (!rsdp_address) return false;

    const uint8_t *bytes = (const uint8_t *)rsdp_address;
    if (!acpitbl_sig_eq((const char *)bytes, "RSD PTR ", ACPITBL_RSDP_SIG_LEN)) {
        return false;
    }

    const acpitbl_rsdp_t *rsdp = (const acpitbl_rsdp_t *)rsdp_address;

    /* The ACPI 1.0 checksum always covers the first 20 bytes. */
    if (acpitbl_sum(bytes, ACPITBL_RSDP_V1_LEN) != 0u) return false;

    bool extended = rsdp->revision >= 2u;
    if (extended) {
        if (rsdp->length < ACPITBL_RSDP_MIN_LEN) {
            extended = false;               /* malformed extended RSDP */
        } else if (acpitbl_sum(bytes, rsdp->length) != 0u) {
            return false;                   /* extended checksum bad  */
        }
    }

    /* Follow the 64-bit pointer when present, otherwise the 32-bit one. */
    uint64_t root_address;
    bool xsdt;
    if (extended && rsdp->xsdt_address != 0u) {
        root_address = rsdp->xsdt_address;
        xsdt = true;
    } else {
        root_address = rsdp->rsdt_address;
        xsdt = false;
    }

    /* Validate the root table and count its entries when reachable. */
    uint32_t entry_count = 0u;
    if (root_address != 0u) {
        const acpitbl_header_t *root =
            (const acpitbl_header_t *)(uintptr_t)root_address;
        const char *expected = xsdt ? ACPITBL_XSDT_SIG : ACPITBL_RSDT_SIG;
        if (acpitbl_sig_eq(root->signature, expected, ACPITBL_SIG_LEN) &&
            root->length >= (uint32_t)sizeof(acpitbl_header_t) &&
            acpitbl_checksum_ok(root, root->length)) {
            uint32_t entry_size = xsdt ? 8u : 4u;
            entry_count = (root->length - (uint32_t)sizeof(acpitbl_header_t)) /
                          entry_size;
        }
    }

    if (info) {
        info->rsdp_valid   = true;
        info->xsdt         = xsdt;
        info->revision     = rsdp->revision;
        info->reserved[0]  = 0u;
        info->reserved[1]  = 0u;
        info->reserved[2]  = 0u;
        info->entry_count  = entry_count;
        info->root_address = root_address;
        info->rsdp_address = (uint64_t)(uintptr_t)rsdp_address;
    }
    return true;
}

bool acpitbl_init(const void *rsdp_address)
{
    acpitbl_initialized = false;
    acpitbl_root = NULL;

    /* Zero the state block without relying on libc. */
    uint8_t *state = (uint8_t *)&acpitbl_state;
    for (size_t i = 0; i < sizeof(acpitbl_state); ++i) state[i] = 0u;

    if (!acpitbl_validate_rsdp(rsdp_address, &acpitbl_state)) return false;

    if (acpitbl_state.root_address != 0u) {
        acpitbl_root =
            (const acpitbl_header_t *)(uintptr_t)acpitbl_state.root_address;
    }

    acpitbl_initialized = true;
    return true;
}

const acpitbl_header_t *acpitbl_find_table(const char *signature)
{
    if (!acpitbl_initialized || !acpitbl_root || !signature) return NULL;

    /* The root table may itself match the requested signature. */
    if (acpitbl_sig_eq(acpitbl_root->signature, signature, ACPITBL_SIG_LEN)) {
        return acpitbl_root;
    }

    const uint8_t *entries =
        (const uint8_t *)acpitbl_root + sizeof(acpitbl_header_t);

    for (uint32_t i = 0u; i < acpitbl_state.entry_count; ++i) {
        uint64_t address;
        if (acpitbl_state.xsdt) {
            address = acpitbl_load_u64(entries + ((size_t)i * 8u));
        } else {
            address = (uint64_t)acpitbl_load_u32(entries + ((size_t)i * 4u));
        }
        if (address == 0u) continue;

        const acpitbl_header_t *header =
            (const acpitbl_header_t *)(uintptr_t)address;
        if (acpitbl_sig_eq(header->signature, signature, ACPITBL_SIG_LEN)) {
            return header;
        }
    }
    return NULL;
}

uint32_t acpitbl_entry_count(void)
{
    if (!acpitbl_initialized) return 0u;
    return acpitbl_state.entry_count;
}

const acpitbl_header_t *acpitbl_root_table(void)
{
    if (!acpitbl_initialized) return NULL;
    return acpitbl_root;
}
