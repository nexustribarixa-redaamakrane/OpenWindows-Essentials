/*
 * owdev64.c - OpenWindows Device Manager Library (.owd)
 *
 * A fixed-capacity device registry. Handles are monotonically increasing
 * so a handle never aliases a later registration; slots are reused once a
 * device is unregistered, which keeps the table dense for enumeration.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdev64.h"

/* ── Internal state ───────────────────────────────────────────── */

typedef struct {
    bool          in_use;
    owdev_id_t    id;
    owdev_desc_t  desc;
} owdev_slot_t;

static owdev_slot_t owdev64_table[OWDEV64_MAX_DEVICES];
static owdev_id_t   owdev64_next_id = 1u;
static bool         owdev64_ready   = false;
static uint32_t     owdev64_count   = 0u;

/* ── String helpers ───────────────────────────────────────────── */

static size_t ow_strlen(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}

static int32_t ow_strcmp(const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] != '\0' && a[i] == b[i]) {
        ++i;
    }
    return (int32_t)(uint8_t)a[i] - (int32_t)(uint8_t)b[i];
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

/* ── Helpers ──────────────────────────────────────────────────── */

static owdev_slot_t *owdev64_find_slot(owdev_id_t id)
{
    for (uint32_t i = 0; i < OWDEV64_MAX_DEVICES; ++i) {
        if (owdev64_table[i].in_use && owdev64_table[i].id == id) {
            return &owdev64_table[i];
        }
    }
    return NULL;
}

static void owdev64_copy_record(const owdev_slot_t *slot, owdev_record_t *out)
{
    out->id        = slot->id;
    out->kind      = slot->desc.kind;
    out->vendor_id = slot->desc.vendor_id;
    out->device_id = slot->desc.device_id;
    out->bus       = slot->desc.bus;
    out->slot      = slot->desc.slot;
    out->irq       = slot->desc.irq;
    out->flags     = slot->desc.flags;
    ow_strncpy(out->name, slot->desc.name, OWDEV64_NAME_MAX);
}

/* ── Implementation ───────────────────────────────────────────── */

void owdev_init(void)
{
    for (uint32_t i = 0; i < OWDEV64_MAX_DEVICES; ++i) {
        owdev64_table[i].in_use = false;
        owdev64_table[i].id     = OWDEV64_INVALID_ID;
        owdev64_table[i].desc.name[0] = '\0';
        owdev64_table[i].desc.kind      = OWDEV_KIND_UNKNOWN;
        owdev64_table[i].desc.vendor_id = 0u;
        owdev64_table[i].desc.device_id = 0u;
        owdev64_table[i].desc.bus       = 0u;
        owdev64_table[i].desc.slot      = 0u;
        owdev64_table[i].desc.irq       = 0u;
        owdev64_table[i].desc.flags     = 0u;
    }
    owdev64_next_id = 1u;
    owdev64_count   = 0u;
    owdev64_ready   = true;
}

owdev_id_t owdev_register(const owdev_desc_t *desc)
{
    if (!owdev64_ready || desc == NULL) {
        return OWDEV64_INVALID_ID;
    }
    if (desc->name[0] == '\0') {
        return OWDEV64_INVALID_ID;
    }
    /* Reject names that cannot be stored without truncation. */
    if (ow_strlen(desc->name) >= (size_t)OWDEV64_NAME_MAX) {
        return OWDEV64_INVALID_ID;
    }

    /* Names must be unique across the registry. */
    for (uint32_t i = 0; i < OWDEV64_MAX_DEVICES; ++i) {
        if (owdev64_table[i].in_use &&
            ow_strcmp(owdev64_table[i].desc.name, desc->name) == 0) {
            return OWDEV64_INVALID_ID;
        }
    }

    for (uint32_t i = 0; i < OWDEV64_MAX_DEVICES; ++i) {
        if (!owdev64_table[i].in_use) {
            owdev_slot_t *slot = &owdev64_table[i];
            owdev_id_t id = owdev64_next_id;
            if (id == OWDEV64_INVALID_ID) {
                id = ++owdev64_next_id;      /* skip the invalid id 0 */
            }
            ++owdev64_next_id;

            slot->in_use = true;
            slot->id     = id;
            slot->desc.kind      = desc->kind;
            slot->desc.vendor_id = desc->vendor_id;
            slot->desc.device_id = desc->device_id;
            slot->desc.bus       = desc->bus;
            slot->desc.slot      = desc->slot;
            slot->desc.irq       = desc->irq;
            slot->desc.flags     = desc->flags;
            ow_strncpy(slot->desc.name, desc->name, OWDEV64_NAME_MAX);

            ++owdev64_count;
            return id;
        }
    }
    return OWDEV64_INVALID_ID;
}

bool owdev_unregister(owdev_id_t id)
{
    if (!owdev64_ready || id == OWDEV64_INVALID_ID) {
        return false;
    }
    for (uint32_t i = 0; i < OWDEV64_MAX_DEVICES; ++i) {
        if (owdev64_table[i].in_use && owdev64_table[i].id == id) {
            owdev64_table[i].in_use = false;
            owdev64_table[i].id     = OWDEV64_INVALID_ID;
            if (owdev64_count > 0u) {
                --owdev64_count;
            }
            return true;
        }
    }
    return false;
}

bool owdev_find(const char *name, owdev_record_t *out)
{
    if (!owdev64_ready || name == NULL) {
        return false;
    }
    for (uint32_t i = 0; i < OWDEV64_MAX_DEVICES; ++i) {
        if (owdev64_table[i].in_use &&
            ow_strcmp(owdev64_table[i].desc.name, name) == 0) {
            if (out != NULL) {
                owdev64_copy_record(&owdev64_table[i], out);
            }
            return true;
        }
    }
    return false;
}

bool owdev_find_by_id(owdev_id_t id, owdev_record_t *out)
{
    if (!owdev64_ready || id == OWDEV64_INVALID_ID) {
        return false;
    }
    owdev_slot_t *slot = owdev64_find_slot(id);
    if (slot == NULL) {
        return false;
    }
    if (out != NULL) {
        owdev64_copy_record(slot, out);
    }
    return true;
}

uint32_t owdev_enumerate(owdev_record_t *out, uint32_t capacity)
{
    uint32_t written = 0;

    if (!owdev64_ready) {
        return 0u;
    }
    for (uint32_t i = 0; i < OWDEV64_MAX_DEVICES; ++i) {
        if (!owdev64_table[i].in_use) {
            continue;
        }
        if (out != NULL && written < capacity) {
            owdev64_copy_record(&owdev64_table[i], &out[written]);
        }
        ++written;
    }
    return written;
}

bool owdev_get_info(owdev_id_t id, owdev_record_t *out)
{
    return owdev_find_by_id(id, out);
}
