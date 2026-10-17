/*
 * owreg64.c - OpenWindows Registry Access Library (.owd)
 *
 * Keys live in a flat, dot-separated namespace held entirely inside the
 * caller's owreg_entry_t array. Lookup is a linear scan; the entry count
 * is kept in the store so callers can reason about capacity.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owreg64.h"

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

static int32_t ow_strncmp(const char *a, const char *b, size_t n)
{
    size_t i = 0;
    while (i < n) {
        if (a[i] != b[i]) {
            return (int32_t)(uint8_t)a[i] - (int32_t)(uint8_t)b[i];
        }
        if (a[i] == '\0') {
            return 0;
        }
        ++i;
    }
    return 0;
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

/* ── Key validation ───────────────────────────────────────────── */

static bool ow_key_char_ok(char c)
{
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        return true;
    }
    if (c >= '0' && c <= '9') {
        return true;
    }
    return (c == '.' || c == '_' || c == '-');
}

/* Accept non-empty, bounded keys of allowed characters that contain no
 * empty dot-separated segments (no leading/trailing/double dot). */
static bool ow_key_valid(const char *key)
{
    size_t len;

    if (key == NULL || key[0] == '\0') {
        return false;
    }
    len = ow_strlen(key);
    if (len >= (size_t)OWREG64_KEY_MAX) {
        return false;
    }
    if (key[0] == '.' || key[len - 1u] == '.') {
        return false;
    }
    for (size_t i = 0; i < len; ++i) {
        if (!ow_key_char_ok(key[i])) {
            return false;
        }
        if (key[i] == '.' && key[i + 1u] == '.') {
            return false;
        }
    }
    return true;
}

/* ── Store helpers ────────────────────────────────────────────── */

/* Linear search for an exact key name; returns slot index or -1. */
static int32_t owreg_find_index(const owreg_store_t *store, const char *key)
{
    if (store == NULL || store->entries == NULL || key == NULL) {
        return -1;
    }
    for (uint32_t i = 0; i < store->capacity; ++i) {
        if (store->entries[i].in_use &&
            ow_strcmp(store->entries[i].key, key) == 0) {
            return (int32_t)i;
        }
    }
    return -1;
}

/* Locate a free slot; returns slot index or -1. */
static int32_t owreg_free_index(const owreg_store_t *store)
{
    for (uint32_t i = 0; i < store->capacity; ++i) {
        if (!store->entries[i].in_use) {
            return (int32_t)i;
        }
    }
    return -1;
}

static owreg_handle_t owreg_bad_handle(void)
{
    owreg_handle_t h;
    h.store    = NULL;
    h.index    = 0u;
    h.writable = false;
    h.valid    = false;
    return h;
}

static owreg_entry_t *owreg_handle_entry(const owreg_handle_t *handle)
{
    if (handle == NULL || !handle->valid || handle->store == NULL) {
        return NULL;
    }
    if (handle->index >= handle->store->capacity) {
        return NULL;
    }
    return &handle->store->entries[handle->index];
}

/* ── Implementation ───────────────────────────────────────────── */

void owreg_store_init(owreg_store_t *store, owreg_entry_t *entries,
                      uint32_t capacity)
{
    if (store == NULL) {
        return;
    }
    store->entries  = entries;
    store->capacity = capacity;
    store->count    = 0u;

    if (entries == NULL) {
        store->capacity = 0u;
        return;
    }
    for (uint32_t i = 0; i < capacity; ++i) {
        entries[i].in_use   = false;
        entries[i].key[0]   = '\0';
        entries[i].value[0] = '\0';
    }
}

owreg_handle_t owreg_open(owreg_store_t *store, const char *key,
                          uint32_t flags)
{
    owreg_handle_t handle = owreg_bad_handle();
    int32_t        index;

    if (store == NULL || store->entries == NULL || !ow_key_valid(key)) {
        return handle;
    }

    index = owreg_find_index(store, key);
    if (index < 0) {
        owreg_entry_t *entry;

        if ((flags & OWREG_OPEN_CREATE) == 0u) {
            return handle;
        }
        index = owreg_free_index(store);
        if (index < 0) {
            return handle;
        }
        entry = &store->entries[index];
        entry->in_use   = true;
        entry->value[0] = '\0';
        ow_strncpy(entry->key, key, OWREG64_KEY_MAX);
        ++store->count;
    }

    handle.store    = store;
    handle.index    = (uint32_t)index;
    handle.writable = (flags & OWREG_OPEN_WRITE) != 0u;
    handle.valid    = true;
    return handle;
}

bool owreg_close(owreg_handle_t *handle)
{
    if (handle == NULL || !handle->valid) {
        return false;
    }
    handle->valid    = false;
    handle->writable = false;
    handle->store    = NULL;
    handle->index    = 0u;
    return true;
}

int32_t owreg_read(const owreg_handle_t *handle, char *out, size_t out_cap)
{
    const owreg_entry_t *entry = owreg_handle_entry(handle);
    size_t               len;

    if (entry == NULL || out == NULL || out_cap == 0u) {
        return OWREG_ERR_PARAM;
    }
    len = ow_strlen(entry->value);
    if (len + 1u > out_cap) {
        return OWREG_ERR_TOOBIG;
    }
    ow_strncpy(out, entry->value, out_cap);
    return (int32_t)len;
}

int32_t owreg_write(owreg_handle_t *handle, const char *value)
{
    owreg_entry_t *entry = owreg_handle_entry(handle);

    if (entry == NULL || value == NULL) {
        return OWREG_ERR_PARAM;
    }
    if (!handle->writable) {
        return OWREG_ERR_ACCESS;
    }
    if (ow_strlen(value) >= (size_t)OWREG64_VALUE_MAX) {
        return OWREG_ERR_TOOBIG;
    }
    ow_strncpy(entry->value, value, OWREG64_VALUE_MAX);
    return OWREG_OK;
}

bool owreg_delete(owreg_store_t *store, const char *key)
{
    int32_t index = owreg_find_index(store, key);

    if (index < 0) {
        return false;
    }
    store->entries[index].in_use   = false;
    store->entries[index].key[0]   = '\0';
    store->entries[index].value[0] = '\0';
    if (store->count > 0u) {
        --store->count;
    }
    return true;
}

bool owreg_exists(const owreg_store_t *store, const char *key)
{
    return owreg_find_index(store, key) >= 0;
}

uint32_t owreg_enum_keys(const owreg_store_t *store, const char *prefix,
                         uint32_t start, const char **names_out,
                         uint32_t max_names)
{
    uint32_t matches = 0u;
    uint32_t stored  = 0u;
    size_t   plen    = (prefix != NULL) ? ow_strlen(prefix) : 0u;

    if (store == NULL || store->entries == NULL) {
        return 0u;
    }

    for (uint32_t i = 0; i < store->capacity; ++i) {
        const owreg_entry_t *entry = &store->entries[i];

        if (!entry->in_use) {
            continue;
        }
        if (prefix != NULL && ow_strncmp(entry->key, prefix, plen) != 0) {
            continue;
        }

        if (matches >= start && names_out != NULL && stored < max_names) {
            names_out[stored] = entry->key;
            ++stored;
        }
        ++matches;
    }
    return matches;
}
