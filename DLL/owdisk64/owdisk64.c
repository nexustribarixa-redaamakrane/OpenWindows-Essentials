/*
 * owdisk64.c - OpenWindows Disk I/O Abstraction (.owd)
 *
 * The device table stores name, geometry, the pluggable backend vtable
 * and the opaque backend context. Transfers are range-checked against
 * the geometry before the backend callback is invoked so a faulty
 * driver cannot be handed an out-of-range LBA.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdisk64.h"

/* ── Internal state ───────────────────────────────────────────── */

typedef struct {
    bool               present;
    char               name[OWDISK64_NAME_MAX];
    uint32_t           block_size;
    uint64_t           total_blocks;
    bool               read_only;
    void              *backend_ctx;
    owdisk_backend_t   backend;
} owdisk_slot_t;

static owdisk_slot_t owdisk64_table[OWDISK64_MAX_DEVICES];
static bool          owdisk64_ready = false;

/* ── String helpers ───────────────────────────────────────────── */

static size_t ow_strlen(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

/* Bounded copy that always NUL-terminates within dst_cap bytes. */
static void ow_strncpy(char *dst, const char *src, size_t dst_cap)
{
    size_t i = 0;
    if (dst_cap == 0u) {
        return;
    }
    while (i + 1u < dst_cap && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

/* ── Helper functions ─────────────────────────────────────────── */

static owdisk_slot_t *owdisk64_slot(uint32_t device)
{
    if (!owdisk64_ready || device >= OWDISK64_MAX_DEVICES) {
        return NULL;
    }
    if (!owdisk64_table[device].present) {
        return NULL;
    }
    return &owdisk64_table[device];
}

/* Reject transfers that would run past the end of the device. */
static bool owdisk64_in_range(const owdisk_slot_t *slot, uint64_t lba,
                              uint32_t count)
{
    if (count == 0u) {
        return false;
    }
    if (lba > slot->total_blocks) {
        return false;
    }
    if ((uint64_t)count > (slot->total_blocks - lba)) {
        return false;
    }
    return true;
}

static void owdisk64_fill_info(const owdisk_slot_t *slot, owdisk_info_t *out)
{
    out->present      = true;
    out->block_size   = slot->block_size;
    out->total_blocks = slot->total_blocks;
    out->total_bytes  = slot->total_blocks * (uint64_t)slot->block_size;
    out->read_only    = slot->read_only;
    ow_strncpy(out->name, slot->name, OWDISK64_NAME_MAX);
}

/* ── Implementation ───────────────────────────────────────────── */

void owdisk_init(void)
{
    for (uint32_t i = 0; i < OWDISK64_MAX_DEVICES; ++i) {
        owdisk_slot_t *slot = &owdisk64_table[i];
        slot->present     = false;
        slot->name[0]     = '\0';
        slot->block_size  = 0u;
        slot->total_blocks = 0u;
        slot->read_only   = false;
        slot->backend_ctx = NULL;
        slot->backend.read  = NULL;
        slot->backend.write = NULL;
        slot->backend.flush = NULL;
    }
    owdisk64_ready = true;
}

int32_t owdisk_register(const char *name, const owdisk_backend_t *backend,
                        void *backend_ctx, uint32_t block_size,
                        uint64_t total_blocks, bool read_only)
{
    if (!owdisk64_ready || name == NULL || backend == NULL) {
        return OWDISK_ERR_PARAM;
    }
    if (name[0] == '\0' || block_size == 0u || total_blocks == 0u) {
        return OWDISK_ERR_PARAM;
    }
    if (ow_strlen(name) >= (size_t)OWDISK64_NAME_MAX) {
        return OWDISK_ERR_PARAM;
    }

    for (uint32_t i = 0; i < OWDISK64_MAX_DEVICES; ++i) {
        if (owdisk64_table[i].present) {
            continue;
        }
        owdisk_slot_t *slot = &owdisk64_table[i];
        slot->present      = true;
        slot->block_size   = block_size;
        slot->total_blocks = total_blocks;
        slot->read_only    = read_only;
        slot->backend_ctx  = backend_ctx;
        slot->backend      = *backend;
        ow_strncpy(slot->name, name, OWDISK64_NAME_MAX);
        return (int32_t)i;
    }
    return OWDISK_ERR_FULL;
}

int32_t owdisk_read(uint32_t device, uint64_t lba, uint32_t count, void *buf)
{
    owdisk_slot_t *slot = owdisk64_slot(device);

    if (slot == NULL) {
        return OWDISK_ERR_NO_DEV;
    }
    if (buf == NULL) {
        return OWDISK_ERR_PARAM;
    }
    if (!owdisk64_in_range(slot, lba, count)) {
        return OWDISK_ERR_RANGE;
    }
    if (slot->backend.read == NULL) {
        return OWDISK_ERR_IO;
    }
    return (slot->backend.read(slot->backend_ctx, lba, count, buf) == 0)
               ? OWDISK_OK
               : OWDISK_ERR_IO;
}

int32_t owdisk_write(uint32_t device, uint64_t lba, uint32_t count,
                     const void *buf)
{
    owdisk_slot_t *slot = owdisk64_slot(device);

    if (slot == NULL) {
        return OWDISK_ERR_NO_DEV;
    }
    if (buf == NULL) {
        return OWDISK_ERR_PARAM;
    }
    if (slot->read_only) {
        return OWDISK_ERR_READONLY;
    }
    if (!owdisk64_in_range(slot, lba, count)) {
        return OWDISK_ERR_RANGE;
    }
    if (slot->backend.write == NULL) {
        return OWDISK_ERR_IO;
    }
    return (slot->backend.write(slot->backend_ctx, lba, count, buf) == 0)
               ? OWDISK_OK
               : OWDISK_ERR_IO;
}

int32_t owdisk_flush(uint32_t device)
{
    owdisk_slot_t *slot = owdisk64_slot(device);

    if (slot == NULL) {
        return OWDISK_ERR_NO_DEV;
    }
    if (slot->backend.flush == NULL) {
        return OWDISK_ERR_IO;
    }
    return (slot->backend.flush(slot->backend_ctx) == 0)
               ? OWDISK_OK
               : OWDISK_ERR_IO;
}

bool owdisk_get_info(uint32_t device, owdisk_info_t *out)
{
    owdisk_slot_t *slot = owdisk64_slot(device);

    if (slot == NULL) {
        return false;
    }
    if (out != NULL) {
        owdisk64_fill_info(slot, out);
    }
    return true;
}

uint32_t owdisk_enumerate(owdisk_info_t *out, uint32_t capacity)
{
    uint32_t written = 0;

    if (!owdisk64_ready) {
        return 0u;
    }
    for (uint32_t i = 0; i < OWDISK64_MAX_DEVICES; ++i) {
        if (!owdisk64_table[i].present) {
            continue;
        }
        if (out != NULL && written < capacity) {
            owdisk64_fill_info(&owdisk64_table[i], &out[written]);
        }
        ++written;
    }
    return written;
}
