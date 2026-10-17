/*
 * smbios.c - OpenWindows SMBIOS Table Parser (.owc)
 *
 * A 64-bit entry point is preferred over the legacy 32-bit one. The
 * structure table is walked by the length of each formatted area plus
 * the double-NUL terminated string pool that follows it.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "smbios.h"

/* Upper bound on a linear walk when the entry point gives no length. */
#define SMBIOS_SCAN_CAP  0x100000u

/* ── Internal state ───────────────────────────────────────────── */

static const uint8_t *smbios_table;
static const uint8_t *smbios_end;
static uint16_t smbios_structures;
static uint8_t smbios_major;
static uint8_t smbios_minor;
static bool smbios_initialized;

/* ── Helpers ──────────────────────────────────────────────────── */

static uint8_t smbios_sum(const uint8_t *data, size_t length)
{
    uint8_t sum = 0u;
    for (size_t i = 0; i < length; ++i) {
        sum = (uint8_t)(sum + data[i]);
    }
    return sum;
}

static bool smbios_anchor_eq(const uint8_t *p, const char *anchor, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        if (p[i] != (uint8_t)anchor[i]) return false;
    }
    return true;
}

/* Advance past one structure's string pool to the next structure. */
static const uint8_t *smbios_next_structure(const uint8_t *header)
{
    const uint8_t *p = header + header[1];   /* skip formatted area */
    while (p + 1u < smbios_end) {
        if (p[0] == 0u && p[1] == 0u) return p + 2u;
        ++p;
    }
    return smbios_end;
}

/* Derive the table end by walking to the 0x7F terminator. */
static void smbios_compute_end(void)
{
    const uint8_t *p = smbios_table;
    size_t scanned = 0u;

    while (scanned < SMBIOS_SCAN_CAP) {
        const smbios_header_t *header = (const smbios_header_t *)p;
        if (header->type == SMBIOS_STRUCT_END) {
            smbios_end = p + header->length;
            return;
        }
        if (header->length < (uint8_t)sizeof(smbios_header_t)) break;

        const uint8_t *next = p + header->length;
        size_t walked = (size_t)(next - smbios_table);
        while (walked < SMBIOS_SCAN_CAP) {
            if (next[0] == 0u && next[1] == 0u) {
                next += 2u;
                break;
            }
            ++next;
            ++walked;
        }
        scanned = (size_t)(next - smbios_table);
        if (scanned <= (size_t)(p - smbios_table)) break;  /* no progress */
        p = next;
    }
    smbios_end = p;
}

static bool smbios_parse_entry32(const uint8_t *p)
{
    const smbios_eps_t *eps = (const smbios_eps_t *)p;

    if (eps->length < SMBIOS_EPS_LEN) return false;
    if (smbios_sum(p, eps->length) != 0u) return false;
    if (!smbios_anchor_eq((const uint8_t *)eps->intermediate_anchor,
                          SMBIOS_INTERMEDIATE, 5u)) {
        return false;
    }
    /* The intermediate entry point checksum covers bytes 0x10..0x1E. */
    if (smbios_sum(p + 0x10u, 0x0Fu) != 0u) return false;

    uint32_t address = eps->structure_table_address;
    if (address == 0u || eps->structure_table_length == 0u) return false;

    smbios_table      = (const uint8_t *)(uintptr_t)address;
    smbios_end        = smbios_table + eps->structure_table_length;
    smbios_structures = eps->number_of_structures;
    smbios_major      = eps->major_version;
    smbios_minor      = eps->minor_version;
    smbios_initialized = true;
    return true;
}

static bool smbios_parse_entry64(const uint8_t *p)
{
    const smbios_eps3_t *eps = (const smbios_eps3_t *)p;

    if (eps->length < SMBIOS_EPS3_MIN_LEN) return false;
    if (smbios_sum(p, eps->length) != 0u) return false;

    uint64_t address = eps->structure_table_address;
    if (address == 0u) return false;

    smbios_table      = (const uint8_t *)(uintptr_t)address;
    smbios_structures = 0u;
    smbios_major      = eps->major_version;
    smbios_minor      = eps->minor_version;
    smbios_end        = NULL;

    if (eps->structure_table_max_size != 0u) {
        smbios_end = smbios_table + eps->structure_table_max_size;
    } else {
        smbios_compute_end();
    }

    smbios_initialized = true;
    return true;
}

/* ── Public API ───────────────────────────────────────────────── */

bool smbios_checksum_ok(const void *data, size_t length)
{
    if (!data || length == 0u) return false;
    return smbios_sum((const uint8_t *)data, length) == 0u;
}

bool smbios_init(const void *search_base, size_t search_length)
{
    smbios_table = NULL;
    smbios_end = NULL;
    smbios_structures = 0u;
    smbios_major = 0u;
    smbios_minor = 0u;
    smbios_initialized = false;

    if (!search_base || search_length < SMBIOS_EPS3_MIN_LEN) return false;

    const uint8_t *base = (const uint8_t *)search_base;

    /* Entry points sit on 16-byte boundaries; prefer the 64-bit one. */
    for (size_t off = 0u; off + 5u <= search_length; off += 16u) {
        if (smbios_anchor_eq(base + off, SMBIOS_EPS3_ANCHOR, 5u)) {
            if (smbios_parse_entry64(base + off)) return true;
        }
    }
    for (size_t off = 0u; off + 4u <= search_length; off += 16u) {
        if (smbios_anchor_eq(base + off, SMBIOS_EPS_ANCHOR, 4u)) {
            if (smbios_parse_entry32(base + off)) return true;
        }
    }
    return false;
}

const smbios_header_t *smbios_find_table(uint8_t type)
{
    if (!smbios_initialized || !smbios_table || !smbios_end) return NULL;

    const uint8_t *p = smbios_table;
    while (p + sizeof(smbios_header_t) <= smbios_end) {
        const smbios_header_t *header = (const smbios_header_t *)p;
        if (header->type == SMBIOS_STRUCT_END) return NULL;
        if (header->type == type) return header;
        if (header->length < (uint8_t)sizeof(smbios_header_t)) return NULL;
        p = smbios_next_structure(p);
    }
    return NULL;
}

const char *smbios_get_string(const smbios_header_t *header, uint8_t index)
{
    if (!header || index == 0u || !smbios_table || !smbios_end) return NULL;

    const uint8_t *p = (const uint8_t *)header + header->length;
    if (p >= smbios_end) return NULL;

    for (uint8_t current = 1u; current < index; ++current) {
        while (p < smbios_end && *p != 0u) ++p;
        if (p >= smbios_end) return NULL;
        ++p;                                     /* skip the NUL terminator */
        if (p >= smbios_end || *p == 0u) return NULL;  /* end of pool       */
    }

    if (p >= smbios_end) return NULL;
    return (const char *)p;
}

uint32_t smbios_version(void)
{
    return ((uint32_t)smbios_major << 8) | (uint32_t)smbios_minor;
}

const uint8_t *smbios_table_address(void)
{
    return smbios_table;
}

size_t smbios_table_length(void)
{
    if (!smbios_initialized || !smbios_table || !smbios_end) return 0u;
    return (size_t)(smbios_end - smbios_table);
}

uint16_t smbios_structure_count(void)
{
    return smbios_structures;
}

bool smbios_is_initialized(void)
{
    return smbios_initialized;
}
