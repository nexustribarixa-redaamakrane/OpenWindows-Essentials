/*
 * owelf64.c - OpenWindows OWX Executable Loader (.owd)
 *
 * Static validation, section mapping, base relocation, and import
 * resolution for native `.owx` images.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owelf64.h"

/* ── Memory helpers ───────────────────────────────────────────── */

static void owelf_memcpy(uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t i;
    for (i = 0u; i < n; ++i) {
        dst[i] = src[i];
    }
}

static void owelf_memset(uint8_t *dst, uint8_t val, size_t n)
{
    size_t i;
    for (i = 0u; i < n; ++i) {
        dst[i] = val;
    }
}

static void owelf_bzero(void *dst, size_t n)
{
    owelf_memset((uint8_t *)dst, 0u, n);
}

/* ── Little-endian field access ───────────────────────────────── */

static uint16_t owelf_rd_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t owelf_rd_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t owelf_rd_u64(const uint8_t *p)
{
    return (uint64_t)owelf_rd_u32(p) | ((uint64_t)owelf_rd_u32(p + 4) << 32);
}

static void owelf_wr_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void owelf_wr_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void owelf_wr_u64(uint8_t *p, uint64_t v)
{
    owelf_wr_u32(p, (uint32_t)v);
    owelf_wr_u32(p + 4, (uint32_t)(v >> 32));
}

/* ── CRC32c (Castagnoli) ──────────────────────────────────────── */

static uint32_t owelf_crc32c(const uint8_t *data, size_t len)
{
    uint32_t c = 0xFFFFFFFFu;
    size_t i;
    uint32_t bit;

    for (i = 0u; i < len; ++i) {
        c ^= (uint32_t)data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            if (c & 1u) {
                c = (c >> 1) ^ 0x82F63B78u;
            } else {
                c >>= 1;
            }
        }
    }
    return c ^ 0xFFFFFFFFu;
}

uint32_t owelf_compute_checksum(const uint8_t *file, size_t size)
{
    uint32_t c = 0xFFFFFFFFu;
    size_t i;
    uint32_t bit;

    if (!file) {
        return 0u;
    }

    for (i = 0u; i < size; ++i) {
        uint8_t b = file[i];

        /* The stored checksum field itself reads as zero. */
        if (i >= (size_t)OWELF_OFF_CHECKSUM &&
            i < (size_t)OWELF_OFF_CHECKSUM + 4u) {
            b = 0u;
        }
        c ^= (uint32_t)b;
        for (bit = 0u; bit < 8u; ++bit) {
            if (c & 1u) {
                c = (c >> 1) ^ 0x82F63B78u;
            } else {
                c >>= 1;
            }
        }
    }
    return c ^ 0xFFFFFFFFu;
}

/* ── Header / table decoding ──────────────────────────────────── */

static owelf_status_t owelf_parse_header(const uint8_t *file, size_t size,
                                         owelf_header_info_t *info)
{
    uint64_t table_end;
    uint64_t import_end;

    if (!file || !info) {
        return OWELF_ERR_ARG;
    }
    if (size < (size_t)OWELF_HEADER_SIZE) {
        return OWELF_ERR_TRUNCATED;
    }

    info->magic         = owelf_rd_u32(file + OWELF_OFF_MAGIC);
    if (info->magic != OWELF_MAGIC) {
        return OWELF_ERR_MAGIC;
    }

    info->version       = owelf_rd_u32(file + OWELF_OFF_VERSION);
    if (info->version != OWELF_VERSION) {
        return OWELF_ERR_VERSION;
    }

    info->flags         = owelf_rd_u32(file + OWELF_OFF_FLAGS);
    info->entry_point   = owelf_rd_u32(file + OWELF_OFF_ENTRY);
    info->section_count = owelf_rd_u32(file + OWELF_OFF_SECTIONS);
    info->import_count  = owelf_rd_u32(file + OWELF_OFF_IMPORTS);
    info->export_count  = owelf_rd_u32(file + OWELF_OFF_EXPORTS);
    info->image_size    = owelf_rd_u64(file + OWELF_OFF_IMAGE_SIZE);
    info->stack_size    = owelf_rd_u32(file + OWELF_OFF_STACK);
    info->heap_size     = owelf_rd_u32(file + OWELF_OFF_HEAP);
    info->checksum      = owelf_rd_u32(file + OWELF_OFF_CHECKSUM);
    info->subsystem     = owelf_rd_u32(file + OWELF_OFF_SUBSYSTEM);

    if (info->section_count > (uint32_t)OWELF_MAX_SECTIONS) {
        return OWELF_ERR_HEADER;
    }
    if (info->import_count > (uint32_t)OWELF_MAX_IMPORTS) {
        return OWELF_ERR_HEADER;
    }
    if (info->image_size == 0u) {
        return OWELF_ERR_HEADER;
    }
    if ((uint64_t)info->entry_point >= info->image_size) {
        return OWELF_ERR_HEADER;
    }

    table_end = (uint64_t)OWELF_HEADER_SIZE +
                ((uint64_t)info->section_count * (uint64_t)OWELF_SECTION_SIZE);
    if (table_end > (uint64_t)size) {
        return OWELF_ERR_TRUNCATED;
    }
    import_end = table_end +
                 ((uint64_t)info->import_count * (uint64_t)OWELF_IMPORT_SIZE);
    if (import_end > (uint64_t)size) {
        return OWELF_ERR_TRUNCATED;
    }

    return OWELF_OK;
}

static void owelf_decode_section(const owelf_image_t *image, uint32_t index,
                                 owelf_section_t *out)
{
    const uint8_t *e = image->file + (size_t)image->section_table_off +
                       ((size_t)index * (size_t)OWELF_SECTION_SIZE);

    out->type         = owelf_rd_u32(e + 0u);
    out->flags        = owelf_rd_u32(e + 4u);
    out->virtual_addr = owelf_rd_u64(e + 8u);
    out->file_offset  = owelf_rd_u64(e + 16u);
    out->size         = owelf_rd_u64(e + 24u);
    out->checksum     = owelf_rd_u32(e + 32u);
    out->reserved     = owelf_rd_u32(e + 36u);
}

/* ── Validation ───────────────────────────────────────────────── */

owelf_status_t owelf_validate(const uint8_t *file, size_t size,
                              owelf_header_info_t *info)
{
    owelf_header_info_t local;
    owelf_status_t st;
    uint32_t computed;

    if (!file) {
        return OWELF_ERR_ARG;
    }

    st = owelf_parse_header(file, size, &local);
    if (st != OWELF_OK) {
        return st;
    }

    computed = owelf_compute_checksum(file, size);
    if (computed != local.checksum) {
        return OWELF_ERR_CHECKSUM;
    }

    if (info) {
        *info = local;
    }
    return OWELF_OK;
}

/* ── Loading ──────────────────────────────────────────────────── */

owelf_status_t owelf_load(owelf_image_t *image,
                          const uint8_t *file, size_t size,
                          uint8_t *load_buffer, size_t load_capacity)
{
    owelf_header_info_t info;
    owelf_status_t st;
    uint32_t i;

    if (!image || !load_buffer) {
        return OWELF_ERR_ARG;
    }

    owelf_bzero(image, sizeof(*image));

    st = owelf_validate(file, size, &info);
    if (st != OWELF_OK) {
        return st;
    }
    if (info.image_size > (uint64_t)load_capacity) {
        return OWELF_ERR_RANGE;
    }

    image->file        = file;
    image->file_size   = size;
    image->image       = load_buffer;
    image->capacity    = load_capacity;
    image->header      = info;
    image->section_table_off = OWELF_HEADER_SIZE;
    image->import_table_off  = (uint32_t)((uint64_t)OWELF_HEADER_SIZE +
        ((uint64_t)info.section_count * (uint64_t)OWELF_SECTION_SIZE));
    image->string_table_off  = (uint32_t)((uint64_t)image->import_table_off +
        ((uint64_t)info.import_count * (uint64_t)OWELF_IMPORT_SIZE));
    image->delta       = 0;
    image->loaded      = false;

    /* Start from a clean image: covers BSS and any alignment gaps. */
    owelf_memset(load_buffer, 0u, (size_t)info.image_size);

    for (i = 0u; i < info.section_count; ++i) {
        owelf_section_t sec;
        uint64_t vaddr_end;

        owelf_decode_section(image, i, &sec);

        vaddr_end = sec.virtual_addr + sec.size;
        if (vaddr_end < sec.virtual_addr) {
            return OWELF_ERR_SECTION;
        }
        if (vaddr_end > info.image_size) {
            return OWELF_ERR_SECTION;
        }
        if (vaddr_end > (uint64_t)load_capacity) {
            return OWELF_ERR_RANGE;
        }

        if (sec.type == OWELF_SECTION_BSS) {
            owelf_memset(load_buffer + sec.virtual_addr, 0u, (size_t)sec.size);
            continue;
        }

        {
            uint64_t file_end = sec.file_offset + sec.size;

            if (file_end < sec.file_offset || file_end > (uint64_t)size) {
                return OWELF_ERR_SECTION;
            }
            owelf_memcpy(load_buffer + sec.virtual_addr,
                         file + sec.file_offset, (size_t)sec.size);
            if (sec.size > 0u &&
                owelf_crc32c(load_buffer + sec.virtual_addr,
                             (size_t)sec.size) != sec.checksum) {
                return OWELF_ERR_SECTION;
            }
        }
    }

    image->loaded = true;
    return OWELF_OK;
}

/* ── Section access ───────────────────────────────────────────── */

bool owelf_section_at(const owelf_image_t *image, uint32_t index,
                      owelf_section_t *out_section)
{
    if (!image || !out_section || !image->loaded) {
        return false;
    }
    if (index >= image->header.section_count) {
        return false;
    }
    owelf_decode_section(image, index, out_section);
    return true;
}

bool owelf_find_section(const owelf_image_t *image, uint32_t type,
                        owelf_section_t *out_section)
{
    uint32_t i;

    if (!image || !out_section || !image->loaded) {
        return false;
    }
    for (i = 0u; i < image->header.section_count; ++i) {
        owelf_section_t sec;

        owelf_decode_section(image, i, &sec);
        if (sec.type == type) {
            *out_section = sec;
            return true;
        }
    }
    return false;
}

/* ── Base relocations ─────────────────────────────────────────── */

static owelf_status_t owelf_apply_reloc(owelf_image_t *image, uint64_t target,
                                        uint32_t type, int64_t delta)
{
    uint64_t end;
    uint8_t *t;

    switch (type) {
    case OWELF_RELOC_ABSOLUTE:
        return OWELF_OK;
    case OWELF_RELOC_HIGH:
    case OWELF_RELOC_LOW:
        end = target + 2u;
        break;
    case OWELF_RELOC_HIGHLOW:
        end = target + 4u;
        break;
    case OWELF_RELOC_DIR64:
        end = target + 8u;
        break;
    default:
        return OWELF_ERR_RELOC;
    }

    if (end < target || end > image->header.image_size ||
        end > (uint64_t)image->capacity) {
        return OWELF_ERR_RANGE;
    }
    t = image->image + target;

    switch (type) {
    case OWELF_RELOC_HIGH: {
        uint16_t v = owelf_rd_u16(t);
        v = (uint16_t)(v + (uint16_t)((uint64_t)delta >> 16));
        owelf_wr_u16(t, v);
        break;
    }
    case OWELF_RELOC_LOW: {
        uint16_t v = owelf_rd_u16(t);
        v = (uint16_t)(v + (uint16_t)((uint64_t)delta & 0xFFFFu));
        owelf_wr_u16(t, v);
        break;
    }
    case OWELF_RELOC_HIGHLOW: {
        uint32_t v = owelf_rd_u32(t);
        v = (uint32_t)(v + (uint32_t)((uint64_t)delta & 0xFFFFFFFFu));
        owelf_wr_u32(t, v);
        break;
    }
    case OWELF_RELOC_DIR64: {
        uint64_t v = owelf_rd_u64(t);
        v += (uint64_t)delta;
        owelf_wr_u64(t, v);
        break;
    }
    default:
        return OWELF_ERR_RELOC;
    }

    return OWELF_OK;
}

owelf_status_t owelf_relocate(owelf_image_t *image, int64_t delta)
{
    owelf_section_t sec;
    const uint8_t *payload;
    uint64_t len;
    uint64_t pos;

    if (!image || !image->loaded) {
        return OWELF_ERR_NOT_LOADED;
    }

    if (!owelf_find_section(image, OWELF_SECTION_RELOC, &sec)) {
        image->delta += delta;   /* Nothing to fix up. */
        return OWELF_OK;
    }

    payload = image->image + sec.virtual_addr;
    len = sec.size;
    pos = 0u;

    while (pos + 8u <= len) {
        uint32_t page  = owelf_rd_u32(payload + pos);
        uint32_t bsize = owelf_rd_u32(payload + pos + 4u);
        uint64_t count;
        uint64_t k;

        if (bsize < 8u || (pos + (uint64_t)bsize) > len) {
            return OWELF_ERR_RELOC;
        }
        count = ((uint64_t)bsize - 8u) / 2u;
        for (k = 0u; k < count; ++k) {
            uint16_t word = owelf_rd_u16(payload + pos + 8u + (k * 2u));
            uint32_t type = (uint32_t)(word >> 12) & 0x0Fu;
            uint32_t off  = (uint32_t)(word & 0x0FFFu);
            owelf_status_t st = owelf_apply_reloc(image,
                                                  (uint64_t)page + (uint64_t)off,
                                                  type, delta);
            if (st != OWELF_OK) {
                return st;
            }
        }
        pos += (uint64_t)bsize;
    }

    if (pos != len) {
        return OWELF_ERR_RELOC;
    }

    image->delta += delta;
    return OWELF_OK;
}

/* ── Import resolution ────────────────────────────────────────── */

owelf_status_t owelf_resolve_imports(owelf_image_t *image,
                                     owelf_resolver_fn resolver, void *user)
{
    uint32_t i;

    if (!image || !image->loaded) {
        return OWELF_ERR_NOT_LOADED;
    }
    if (!resolver) {
        return OWELF_ERR_ARG;
    }

    for (i = 0u; i < image->header.import_count; ++i) {
        const uint8_t *entry = image->file + image->import_table_off +
                               ((size_t)i * (size_t)OWELF_IMPORT_SIZE);
        owelf_import_t imp;
        uint64_t module_pos;
        const char *module;
        uint32_t j;

        imp.module_name_off  = owelf_rd_u32(entry + 0u);
        imp.symbol_table_off = owelf_rd_u32(entry + 4u);
        imp.iat_off          = owelf_rd_u32(entry + 8u);
        imp.symbol_count     = owelf_rd_u32(entry + 12u);
        imp.checksum         = owelf_rd_u32(entry + 16u);
        imp.reserved         = owelf_rd_u32(entry + 20u);

        module_pos = (uint64_t)image->string_table_off +
                     (uint64_t)imp.module_name_off;
        if (module_pos >= (uint64_t)image->file_size) {
            return OWELF_ERR_IMPORT;
        }
        module = (const char *)(image->file + module_pos);

        for (j = 0u; j < imp.symbol_count; ++j) {
            uint64_t name_slot = (uint64_t)imp.symbol_table_off +
                                 ((uint64_t)j * 4u);
            uint32_t name_off;
            uint64_t name_pos;
            const char *symbol;
            uintptr_t address = 0u;
            uint64_t iat_slot;

            if (name_slot + 4u > (uint64_t)image->file_size) {
                return OWELF_ERR_IMPORT;
            }
            name_off = owelf_rd_u32(image->file + name_slot);

            name_pos = (uint64_t)image->string_table_off + (uint64_t)name_off;
            if (name_pos >= (uint64_t)image->file_size) {
                return OWELF_ERR_IMPORT;
            }
            symbol = (const char *)(image->file + name_pos);

            if (!resolver(module, symbol, &address, user)) {
                return OWELF_ERR_RESOLVE;
            }

            iat_slot = (uint64_t)imp.iat_off +
                       ((uint64_t)j * (uint64_t)OWELF_POINTER_SIZE);
            if (iat_slot + (uint64_t)OWELF_POINTER_SIZE > image->header.image_size ||
                iat_slot + (uint64_t)OWELF_POINTER_SIZE > (uint64_t)image->capacity) {
                return OWELF_ERR_IMPORT;
            }
            owelf_wr_u64(image->image + iat_slot, (uint64_t)address);
        }
    }

    return OWELF_OK;
}

/* ── Entry point ──────────────────────────────────────────────── */

void *owelf_get_entry(const owelf_image_t *image)
{
    if (!image || !image->loaded) {
        return NULL;
    }
    if ((uint64_t)image->header.entry_point >= image->header.image_size ||
        (uint64_t)image->header.entry_point >= (uint64_t)image->capacity) {
        return NULL;
    }
    return (void *)(image->image + image->header.entry_point);
}
