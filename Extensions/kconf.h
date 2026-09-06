/*
 * kconf.h - OpenWindows Hierarchical Configuration Engine (.kconf)
 *
 * A human-readable, Linux-like alternative to the Windows Registry.
 * Configuration keys are hierarchical path strings (e.g. "system/kernel/panic_action"
 * or "drivers/sio/baud_rate") mapped to typed values (string, integer, boolean).
 *
 * Designed for zero dynamic heap allocation: uses static pre-allocated
 * table slots or caller-provided buffers.
 *
 * C99 freestanding.
 */

#ifndef KCONF_H
#define KCONF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define KCONF_MAX_KEY_LEN       128u
#define KCONF_MAX_VAL_LEN       128u
#define KCONF_MAX_ENTRIES       256u

typedef enum {
    KCONF_TYPE_STRING   = 0,
    KCONF_TYPE_INTEGER  = 1,
    KCONF_TYPE_BOOLEAN  = 2,
    KCONF_TYPE_HEX      = 3
} kconf_type_t;

typedef struct {
    char            key[KCONF_MAX_KEY_LEN];
    char            val_str[KCONF_MAX_VAL_LEN];
    int64_t         val_int;
    bool            val_bool;
    kconf_type_t    type;
    bool            is_active;
} kconf_node_t;

typedef struct {
    kconf_node_t    entries[KCONF_MAX_ENTRIES];
    uint32_t        entry_count;
    uint32_t        version;
} kconf_registry_t;

static inline void kconf_init(kconf_registry_t *reg)
{
    if (!reg) return;
    reg->entry_count = 0u;
    reg->version = 1u;
    for (size_t i = 0; i < KCONF_MAX_ENTRIES; ++i) {
        reg->entries[i].is_active = false;
    }
}

static inline bool kconf_streq(const char *a, const char *b)
{
    size_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return false;
        i++;
    }
    return a[i] == b[i];
}

static inline const kconf_node_t *kconf_lookup(const kconf_registry_t *reg, const char *key)
{
    if (!reg || !key) return NULL;
    for (size_t i = 0; i < reg->entry_count; ++i) {
        if (reg->entries[i].is_active && kconf_streq(reg->entries[i].key, key)) {
            return &reg->entries[i];
        }
    }
    return NULL;
}

static inline bool kconf_set_str(kconf_registry_t *reg, const char *key, const char *val)
{
    if (!reg || !key || !val) return false;
    /* Check existing */
    for (size_t i = 0; i < reg->entry_count; ++i) {
        if (reg->entries[i].is_active && kconf_streq(reg->entries[i].key, key)) {
            size_t vi = 0;
            while (vi < KCONF_MAX_VAL_LEN - 1 && val[vi]) {
                reg->entries[i].val_str[vi] = val[vi];
                vi++;
            }
            reg->entries[i].val_str[vi] = '\0';
            reg->entries[i].type = KCONF_TYPE_STRING;
            return true;
        }
    }
    if (reg->entry_count >= KCONF_MAX_ENTRIES) return false;

    kconf_node_t *node = &reg->entries[reg->entry_count];
    size_t ki = 0;
    while (ki < KCONF_MAX_KEY_LEN - 1 && key[ki]) {
        node->key[ki] = key[ki];
        ki++;
    }
    node->key[ki] = '\0';

    size_t vi = 0;
    while (vi < KCONF_MAX_VAL_LEN - 1 && val[vi]) {
        node->val_str[vi] = val[vi];
        vi++;
    }
    node->val_str[vi] = '\0';

    node->type = KCONF_TYPE_STRING;
    node->is_active = true;
    reg->entry_count++;
    return true;
}

static inline bool kconf_set_int(kconf_registry_t *reg, const char *key, int64_t val)
{
    if (!reg || !key) return false;
    for (size_t i = 0; i < reg->entry_count; ++i) {
        if (reg->entries[i].is_active && kconf_streq(reg->entries[i].key, key)) {
            reg->entries[i].val_int = val;
            reg->entries[i].type = KCONF_TYPE_INTEGER;
            return true;
        }
    }
    if (reg->entry_count >= KCONF_MAX_ENTRIES) return false;

    kconf_node_t *node = &reg->entries[reg->entry_count];
    size_t ki = 0;
    while (ki < KCONF_MAX_KEY_LEN - 1 && key[ki]) {
        node->key[ki] = key[ki];
        ki++;
    }
    node->key[ki] = '\0';
    node->val_int = val;
    node->type = KCONF_TYPE_INTEGER;
    node->is_active = true;
    reg->entry_count++;
    return true;
}

#endif /* KCONF_H */
