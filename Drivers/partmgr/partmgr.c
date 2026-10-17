/*
 * partmgr.c - OpenWindows Partition Manager (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation: the GPT entry table is
 * file-scope static storage (the UEFI maximum of 128 entries of 128 bytes
 * fits comfortably). Disk access is delegated to the caller's block reader.
 */

#include "partmgr.h"

/* ── Tiny local helpers ───────────────────────────────────────── */

static void ow_memset(void *dst, int value, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < len; ++i) {
        d[i] = (uint8_t)value;
    }
}

static void ow_memcpy(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < len; ++i) {
        d[i] = s[i];
    }
}

static uint32_t pm_rd32le(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint64_t pm_rd64le(const uint8_t *p)
{
    uint64_t v = 0u;
    for (uint32_t i = 0u; i < 8u; ++i) {
        v |= (uint64_t)p[i] << (8u * i);
    }
    return v;
}

/* Reflected CRC-32 (IEEE 802.3), as used by GPT. */
static uint32_t pm_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (size_t i = 0u; i < len; ++i) {
        crc ^= (uint32_t)data[i];
        for (uint32_t bit = 0u; bit < 8u; ++bit) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static bool pm_guid_is_zero(const uint8_t *guid)
{
    for (uint32_t i = 0u; i < 16u; ++i) {
        if (guid[i] != 0u) {
            return false;
        }
    }
    return true;
}

/* ── Static GPT entry staging area ────────────────────────────── */

#define PARTMGR_GPT_MAX_BYTES  (128u * 128u)
#define PARTMGR_GPT_READ_CHUNK  32u

static uint8_t g_partmgr_gpt[PARTMGR_GPT_MAX_BYTES] __attribute__((aligned(512)));

/* ── MBR parsing ──────────────────────────────────────────────── */

static bool pm_mbr_type_is_extended(uint8_t type)
{
    return (type == 0x05u) || (type == 0x0Fu) || (type == 0x85u);
}

static void pm_store_mbr_entry(partmgr_table_t *table, const uint8_t *entry,
                               uint64_t abs_first)
{
    partmgr_entry_t *dst;
    uint8_t type = entry[4];

    if ((type == 0u) || (type == 0xEEu)) {
        return;   /* empty slot or protective MBR marker */
    }
    if (table->entry_count >= PARTMGR_MAX_ENTRIES) {
        return;
    }

    dst = &table->entries[table->entry_count];
    ow_memset(dst, 0, sizeof(*dst));
    dst->index = table->entry_count + 1u;
    dst->bootable = ((entry[0] & 0x80u) != 0u);
    dst->gpt = false;
    dst->mbr_type = type;
    dst->first_lba = abs_first;
    dst->sector_count = (uint64_t)pm_rd32le(&entry[12]);
    dst->last_lba = (dst->sector_count > 0u)
                  ? (dst->first_lba + dst->sector_count - 1u)
                  : dst->first_lba;
    table->entry_count++;
}

static partmgr_status_t pm_follow_extended(partmgr_t *pm, uint64_t base)
{
    uint8_t ebr[PARTMGR_SECTOR_SIZE];
    uint64_t current = base;

    for (uint32_t link = 0u; link < PARTMGR_MAX_EBR_CHAIN; ++link) {
        const uint8_t *entry0;
        const uint8_t *entry1;

        if (pm->read(pm->ctx, current, 1u, ebr) != 0) {
            return PARTMGR_ERR_IO;
        }
        if ((ebr[510] != 0x55u) || (ebr[511] != 0xAAu)) {
            return PARTMGR_ERR_NO_TABLE;
        }

        entry0 = &ebr[446];
        entry1 = &ebr[462];

        /* The first EBR entry is relative to the current EBR. */
        pm_store_mbr_entry(&pm->table, entry0, current + pm_rd32le(&entry0[8]));

        if (!pm_mbr_type_is_extended(entry1[4])) {
            return PARTMGR_OK;
        }
        /* The next EBR link is relative to the extended partition base. */
        current = base + pm_rd32le(&entry1[8]);
    }
    return PARTMGR_ERR_NO_TABLE;
}

partmgr_status_t partmgr_read_mbr(partmgr_t *pm, const uint8_t *sector)
{
    bool saw_signature;
    uint32_t extended_count = 0u;
    uint64_t extended_base = 0u;

    if ((pm == NULL) || !pm->initialized || (sector == NULL)) {
        return PARTMGR_ERR_INVALID_PARAM;
    }

    ow_memset(&pm->table, 0, sizeof(pm->table));
    pm->table.style = PARTMGR_STYLE_MBR;

    saw_signature = (sector[510] == 0x55u) && (sector[511] == 0xAAu);
    if (!saw_signature) {
        return PARTMGR_ERR_NO_TABLE;
    }
    pm->table.valid = true;

    for (uint32_t i = 0u; i < 4u; ++i) {
        const uint8_t *entry = &sector[446u + (i * 16u)];
        uint8_t type = entry[4];

        if (type == 0xEEu) {
            pm->table.protective_mbr = true;
            continue;
        }
        if (pm_mbr_type_is_extended(type)) {
            extended_count++;
            extended_base = (uint64_t)pm_rd32le(&entry[8]);
            continue;
        }
        pm_store_mbr_entry(&pm->table, entry,
                           (uint64_t)pm_rd32le(&entry[8]));
    }

    if (extended_count > 0u) {
        (void)pm_follow_extended(pm, extended_base);
    }
    return PARTMGR_OK;
}

/* ── GPT parsing ──────────────────────────────────────────────── */

static partmgr_status_t pm_read_gpt_entries(partmgr_t *pm, uint64_t entries_lba,
                                            uint32_t total_bytes)
{
    uint32_t sectors = (total_bytes + (PARTMGR_SECTOR_SIZE - 1u))
                     / PARTMGR_SECTOR_SIZE;
    uint32_t off = 0u;

    if (total_bytes > PARTMGR_GPT_MAX_BYTES) {
        return PARTMGR_ERR_NO_SPACE;
    }

    while (off < sectors) {
        uint32_t chunk = sectors - off;
        if (chunk > PARTMGR_GPT_READ_CHUNK) {
            chunk = PARTMGR_GPT_READ_CHUNK;
        }
        if (pm->read(pm->ctx, entries_lba + (uint64_t)off, chunk,
                     &g_partmgr_gpt[off * PARTMGR_SECTOR_SIZE]) != 0) {
            return PARTMGR_ERR_IO;
        }
        off += chunk;
    }
    return PARTMGR_OK;
}

partmgr_status_t partmgr_read_gpt(partmgr_t *pm)
{
    uint8_t header[PARTMGR_SECTOR_SIZE];
    uint8_t header_copy[PARTMGR_SECTOR_SIZE];
    uint32_t header_size;
    uint32_t header_crc;
    uint32_t entries_crc;
    uint32_t entry_count;
    uint32_t entry_size;
    uint64_t entries_lba;
    uint32_t total_bytes;
    partmgr_status_t status;

    if ((pm == NULL) || !pm->initialized) {
        return PARTMGR_ERR_INVALID_PARAM;
    }

    if (pm->read(pm->ctx, 1u, 1u, header) != 0) {
        return PARTMGR_ERR_IO;
    }
    if ((header[0] != 'E') || (header[1] != 'F') || (header[2] != 'I')
        || (header[3] != ' ')) {
        return PARTMGR_ERR_NO_TABLE;
    }
    if ((header[4] != 'P') || (header[5] != 'A') || (header[6] != 'R')
        || (header[7] != 'T')) {
        return PARTMGR_ERR_NO_TABLE;
    }

    header_size = pm_rd32le(&header[12]);
    if ((header_size < 92u) || (header_size > PARTMGR_SECTOR_SIZE)) {
        return PARTMGR_ERR_CRC;
    }

    /* Validate the header CRC over `header_size` bytes with CRC zeroed. */
    ow_memcpy(header_copy, header, PARTMGR_SECTOR_SIZE);
    header_crc = pm_rd32le(&header[16]);
    ow_memset(&header_copy[16], 0, 4u);
    if (pm_crc32(header_copy, header_size) != header_crc) {
        return PARTMGR_ERR_CRC;
    }

    entry_count = pm_rd32le(&header[80]);
    entry_size = pm_rd32le(&header[84]);
    entries_crc = pm_rd32le(&header[88]);
    entries_lba = pm_rd64le(&header[72]);

    if ((entry_size < PARTMGR_GPT_ENTRY_SIZE) || (entry_count == 0u)) {
        return PARTMGR_ERR_NO_TABLE;
    }
    if (entry_count > (PARTMGR_GPT_MAX_BYTES / entry_size)) {
        return PARTMGR_ERR_NO_SPACE;
    }
    total_bytes = entry_count * entry_size;

    status = pm_read_gpt_entries(pm, entries_lba, total_bytes);
    if (status != PARTMGR_OK) {
        return status;
    }

    if (pm_crc32(g_partmgr_gpt, total_bytes) != entries_crc) {
        return PARTMGR_ERR_CRC;
    }

    ow_memset(&pm->table, 0, sizeof(pm->table));
    pm->table.style = PARTMGR_STYLE_GPT;
    pm->table.valid = true;
    pm->table.protective_mbr = true;
    ow_memcpy(pm->table.disk_guid, &header[56], 16u);

    for (uint32_t i = 0u; (i < entry_count) && (i < PARTMGR_MAX_ENTRIES); ++i) {
        const uint8_t *src = &g_partmgr_gpt[i * entry_size];
        partmgr_entry_t *dst;

        if (pm_guid_is_zero(src)) {
            continue;
        }
        dst = &pm->table.entries[pm->table.entry_count];
        ow_memset(dst, 0, sizeof(*dst));
        dst->index = pm->table.entry_count + 1u;
        dst->gpt = true;
        ow_memcpy(dst->type_guid, &src[0], 16u);
        ow_memcpy(dst->unique_guid, &src[16], 16u);
        dst->first_lba = pm_rd64le(&src[32]);
        dst->last_lba = pm_rd64le(&src[40]);
        dst->sector_count = (dst->last_lba >= dst->first_lba)
                          ? (dst->last_lba - dst->first_lba + 1u)
                          : 0u;
        for (uint32_t c = 0u; c < PARTMGR_GPT_NAME_UNITS; ++c) {
            dst->name[c] = (uint16_t)src[56u + (c * 2u)]
                         | (uint16_t)((uint16_t)src[57u + (c * 2u)] << 8);
        }
        pm->table.entry_count++;
    }
    return PARTMGR_OK;
}

/* ── Public API ───────────────────────────────────────────────── */

partmgr_status_t partmgr_init(partmgr_t *pm, partmgr_read_fn read_fn, void *ctx)
{
    if ((pm == NULL) || (read_fn == NULL)) {
        return PARTMGR_ERR_INVALID_PARAM;
    }

    ow_memset(pm, 0, sizeof(*pm));
    pm->read = read_fn;
    pm->ctx = ctx;
    pm->table.style = PARTMGR_STYLE_NONE;
    pm->initialized = true;
    return PARTMGR_OK;
}

partmgr_status_t partmgr_scan(partmgr_t *pm)
{
    uint8_t sector[PARTMGR_SECTOR_SIZE];
    partmgr_status_t status;

    if ((pm == NULL) || !pm->initialized) {
        return PARTMGR_ERR_NOT_INITIALIZED;
    }

    if (pm->read(pm->ctx, 0u, 1u, sector) != 0) {
        return PARTMGR_ERR_IO;
    }

    status = partmgr_read_mbr(pm, sector);
    if (status != PARTMGR_OK) {
        return status;
    }

    if (pm->table.protective_mbr) {
        /* A protective MBR means a GPT is expected at LBA 1. Keep the MBR
         * result as a fallback when the GPT turns out to be unreadable. */
        (void)partmgr_read_gpt(pm);
    }
    return PARTMGR_OK;
}
