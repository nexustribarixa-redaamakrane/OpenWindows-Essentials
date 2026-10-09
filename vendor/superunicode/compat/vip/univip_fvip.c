/* univip_fvip.c - Volume Indexing Protocols (VIPs) Implementation
 *
 * Pure C99 freestanding implementation of the VIP framework:
 *   - VIP general infrastructure (volume registry)
 *   - FVIP (File Volume Indexing Protocol) file-level indexing
 *   - UniVIP (Universal Volume Indexing Protocol) 44-bit hex trie
 *
 * Zero dynamic allocation. All pools are pre-allocated static arrays
 * passed in via initialization parameters.
 */
#include "univip_fvip.h"

/* =========================================================================
 * Freestanding Utility Helpers
 * ========================================================================= */

static size_t vip_strlen(const char *s) {
    size_t n = 0;
    if (!s) {
        return 0;
    }
    while (s[n] != '\0') {
        ++n;
    }
    return n;
}


static void *vip_memcpy(void *dest, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    if (!dest || !src) {
        return dest;
    }
    for (i = 0; i < n; ++i) {
        d[i] = s[i];
    }
    return dest;
}

static void *vip_memset(void *dest, int val, size_t n) {
    uint8_t *d = (uint8_t *)dest;
    uint8_t v = (uint8_t)val;
    size_t i;
    if (!dest) {
        return dest;
    }
    for (i = 0; i < n; ++i) {
        d[i] = v;
    }
    return dest;
}


/* =========================================================================
 * Global VIP System State
 * ========================================================================= */

static vip_registry_t g_vip_registry;

/* =========================================================================
 * Path Hash - FNV-1a, masked to 44 bits
 * ========================================================================= */

static uint64_t fvip_hash_path(const char *path, size_t len) {
    uint64_t h = 0xCBF29CE484222325ULL;
    size_t i;
    for (i = 0; i < len; ++i) {
        h ^= (uint64_t)(uint8_t)path[i];
        h *= 0x100000001B3ULL;
    }
    return h & 0xFFFFFFFFFFFULL;
}

/* =========================================================================
 * SuperUnicode / SUTF-8 Compatibility Codec
 *
 * Freestanding reimplementation of the libsutf transport rules:
 * 1-6 byte streams, overlong rejection, trap-range & sentinel exclusion.
 * ========================================================================= */

static bool fvip_sucs_valid(uint32_t cp) {
    if (cp > SUCS_MAX_CODEPOINT) {
        return false;
    }
    if (cp >= (uint32_t)SUCS_TRAP_RANGE_MIN && cp <= (uint32_t)SUCS_TRAP_RANGE_MAX) {
        return false;
    }
    if (cp == SUCS_INVALID_CODEPOINT) {
        return false;
    }
    return true;
}

size_t fvip_sutf8_codepoint_length(uint32_t cp) {
    if (!fvip_sucs_valid(cp)) {
        return 0;
    }
    if (cp <= 0x7FUL) {
        return 1;
    } else if (cp <= 0x7FFUL) {
        return 2;
    } else if (cp <= 0xFFFFUL) {
        return 3;
    } else if (cp <= 0x0010FFFFUL) {
        return 4;
    } else if (cp <= 0x03FFFFFFUL) {
        return 5;
    }
    return 6;
}

size_t fvip_sutf8_decode_char(const uint8_t *in_buf, size_t buf_size,
                              uint32_t *out_cp) {
    uint32_t cp = SUCS_INVALID_CODEPOINT;
    uint8_t b0;
    size_t len;
    size_t i;

    if (out_cp) {
        *out_cp = SUCS_INVALID_CODEPOINT;
    }
    if (!in_buf || buf_size == 0) {
        return 0;
    }

    b0 = in_buf[0];
    if (b0 < 0x80U) {
        cp = (uint32_t)b0;
        len = 1;
    } else if ((b0 & 0xE0U) == 0xC0U) {
        cp = (uint32_t)(b0 & 0x1FU);
        len = 2;
    } else if ((b0 & 0xF0U) == 0xE0U) {
        cp = (uint32_t)(b0 & 0x0FU);
        len = 3;
    } else if ((b0 & 0xF8U) == 0xF0U) {
        cp = (uint32_t)(b0 & 0x07U);
        len = 4;
    } else if ((b0 & 0xFCU) == 0xF8U) {
        cp = (uint32_t)(b0 & 0x03U);
        len = 5;
    } else if ((b0 & 0xFEU) == 0xFCU) {
        cp = (uint32_t)(b0 & 0x01U);
        len = 6;
    } else {
        return 0;
    }

    if (buf_size < len) {
        return 0;
    }
    for (i = 1; i < len; ++i) {
        if ((in_buf[i] & 0xC0U) != 0x80U) {
            return 0;
        }
        cp = (cp << 6) | (uint32_t)(in_buf[i] & 0x3FU);
    }

    /* Overlong encodings and out-of-encoding-space codepoints rejected */
    if (!fvip_sucs_valid(cp) || fvip_sutf8_codepoint_length(cp) != len) {
        return 0;
    }

    if (out_cp) {
        *out_cp = cp;
    }
    return len;
}

bool fvip_str_is_sutf8(const char *s) {
    const uint8_t *p;
    size_t consumed;

    if (!s) {
        return false;
    }
    p = (const uint8_t *)s;
    while (*p != 0U) {
        consumed = fvip_sutf8_decode_char(p, FVIP_PATH_MAX, (void *)0);
        if (consumed == 0) {
            return false;
        }
        p += consumed;
    }
    return true;
}

uint32_t fvip_str_codepoint_count(const char *s, size_t *out_count) {
    const uint8_t *p;
    size_t consumed;
    size_t count = 0;

    if (!out_count) {
        return VIP_ERR_NULL_POINTER;
    }
    *out_count = 0;
    if (!s) {
        return VIP_ERR_NULL_POINTER;
    }

    p = (const uint8_t *)s;
    while (*p != 0U) {
        consumed = fvip_sutf8_decode_char(p, FVIP_PATH_MAX, (void *)0);
        if (consumed == 0) {
            return VIP_ERR_INVALID_SUTF8;
        }
        p += consumed;
        ++count;
    }

    *out_count = count;
    return VIP_OK;
}

/* =========================================================================
 * UniVIP Hex Trie - Internal Operations
 * ========================================================================= */

/* Extract the nibble at trie level (0 = MSB, 10 = LSB) */
static uint8_t univip_nibble(uint64_t key, int level) {
    return (uint8_t)((key >> (4 * (UNIVIP_NIBBLE_COUNT - 1 - level))) & 0x0FULL);
}

/* Cast const away from table to access trie internals.
 * The trie is embedded in the reserved space of fvip_table_t.
 * This is the only way to access it without dynamic allocation. */
static univip_tree_t *univip_get_tree(fvip_table_t *table) {
    return (univip_tree_t *)(void *)table->_trie_reserved;
}

static const univip_tree_t *univip_get_tree_const(const fvip_table_t *table) {
    return (const univip_tree_t *)(const void *)table->_trie_reserved;
}

/* Initialize the hex trie within an FVIP table */
static void univip_tree_init(univip_tree_t *tree) {
    uint32_t i;
    vip_memset(tree, 0, sizeof(univip_tree_t));
    for (i = 0; i < UNIVIP_MAX_NODES; ++i) {
        tree->free_stack[i] = UNIVIP_MAX_NODES - 1 - i;
        tree->nodes[i].entry_index = UNIVIP_NULL_ENTRY;
    }
    tree->free_top = UNIVIP_MAX_NODES;
    tree->root = UNIVIP_NULL_NODE;
    tree->node_count = 0;
    tree->entry_count = 0;
}

/* Allocate a node from the pool. Returns node index or UNIVIP_NULL_NODE. */
static uint32_t univip_node_alloc(univip_tree_t *tree) {
    uint32_t idx;
    if (tree->free_top == 0) {
        return UNIVIP_NULL_NODE;
    }
    --tree->free_top;
    idx = tree->free_stack[tree->free_top];
    vip_memset(&tree->nodes[idx], 0, sizeof(univip_node_t));
    tree->nodes[idx].entry_index = UNIVIP_NULL_ENTRY;
    ++tree->node_count;
    return idx;
}

/* Free a node back to the pool */
static void univip_node_free(univip_tree_t *tree, uint32_t idx) {
    if (idx < UNIVIP_MAX_NODES && tree->node_count > 0) {
        vip_memset(&tree->nodes[idx], 0, sizeof(univip_node_t));
        tree->nodes[idx].entry_index = UNIVIP_NULL_ENTRY;
        tree->free_stack[tree->free_top] = idx;
        ++tree->free_top;
        --tree->node_count;
    }
}

/* Read-only trie walk: traverse to the node at the given key without
 * any mutation. Returns the node index, or UNIVIP_NULL_NODE if not found. */
static uint32_t univip_walk_const(const univip_tree_t *tree, uint64_t key) {
    uint32_t current;
    uint32_t level;
    uint8_t nib;
    uint32_t child_idx;

    if (tree->root == UNIVIP_NULL_NODE) {
        return UNIVIP_NULL_NODE;
    }

    current = tree->root;

    for (level = 0; level < UNIVIP_NIBBLE_COUNT; ++level) {
        nib = univip_nibble(key, level);

        if (tree->nodes[current].child_bitmap & ((uint16_t)1U << nib)) {
            child_idx = tree->nodes[current].children[nib];
            if (child_idx >= UNIVIP_MAX_NODES) {
                return UNIVIP_NULL_NODE;
            }
            current = child_idx;
        } else {
            return UNIVIP_NULL_NODE;
        }
    }

    return current;
}

/* Walk the trie to find the node at the given key.
 * If create is true, intermediate nodes are allocated as needed.
 * Returns the node index, or UNIVIP_NULL_NODE if not found / allocation failed. */
static uint32_t univip_walk(univip_tree_t *tree, uint64_t key, bool create) {
    uint32_t current;
    uint32_t level;
    uint8_t nib;
    uint32_t child_idx;

    if (tree->root == UNIVIP_NULL_NODE) {
        if (!create) {
            return UNIVIP_NULL_NODE;
        }
        tree->root = univip_node_alloc(tree);
        if (tree->root == UNIVIP_NULL_NODE) {
            return UNIVIP_NULL_NODE;
        }
    }

    current = tree->root;

    for (level = 0; level < UNIVIP_NIBBLE_COUNT; ++level) {
        nib = univip_nibble(key, level);

        if (tree->nodes[current].child_bitmap & ((uint16_t)1U << nib)) {
            child_idx = tree->nodes[current].children[nib];
            if (child_idx >= UNIVIP_MAX_NODES) {
                return UNIVIP_NULL_NODE;
            }
            current = child_idx;
        } else {
            if (!create) {
                return UNIVIP_NULL_NODE;
            }
            child_idx = univip_node_alloc(tree);
            if (child_idx == UNIVIP_NULL_NODE) {
                return UNIVIP_NULL_NODE;
            }
            tree->nodes[current].child_bitmap |= (uint16_t)1U << nib;
            tree->nodes[current].children[nib] = child_idx;
            current = child_idx;
        }
    }

    return current;
}

/* Remove a node and prune empty ancestors up to the root.
 * The key is needed to retrace the path during pruning. */
static void univip_prune(univip_tree_t *tree, uint64_t key) {
    uint32_t path_nodes[UNIVIP_NIBBLE_COUNT + 1];
    uint32_t level;
    uint8_t nib;

    /* Retrace: store the full path from root to the terminal node.
     * The walk traverses UNIVIP_NIBBLE_COUNT edges, producing
     * UNIVIP_NIBBLE_COUNT + 1 nodes (root at depth 0, terminal at depth 11). */
    if (tree->root == UNIVIP_NULL_NODE) {
        return;
    }

    path_nodes[0] = tree->root;
    for (level = 0; level < UNIVIP_NIBBLE_COUNT; ++level) {
        nib = univip_nibble(key, level);
        if (!(tree->nodes[path_nodes[level]].child_bitmap & ((uint16_t)1U << nib))) {
            return;
        }
        path_nodes[level + 1] = tree->nodes[path_nodes[level]].children[nib];
    }

    /* Walk bottom-up from terminal to root, removing empty nodes */
    for (level = UNIVIP_NIBBLE_COUNT; level >= 1; --level) {
        uint32_t node = path_nodes[level];
        uint32_t parent = path_nodes[level - 1];
        nib = univip_nibble(key, level - 1);

        if (tree->nodes[node].child_bitmap == 0 &&
            tree->nodes[node].entry_index == UNIVIP_NULL_ENTRY) {
            tree->nodes[parent].child_bitmap &= (uint16_t)~((uint16_t)1U << nib);
            tree->nodes[parent].children[nib] = UNIVIP_NULL_NODE;
            univip_node_free(tree, node);
        }
    }

    /* Check root */
    if (tree->nodes[tree->root].child_bitmap == 0 &&
        tree->nodes[tree->root].entry_index == UNIVIP_NULL_ENTRY) {
        univip_node_free(tree, tree->root);
        tree->root = UNIVIP_NULL_NODE;
    }
}

/* =========================================================================
 * VIP General Infrastructure - Initialization
 * ========================================================================= */

uint32_t univip_init_system(void) {
    vip_memset(&g_vip_registry, 0, sizeof(vip_registry_t));
    g_vip_registry.initialized = true;
    return VIP_INIT_OK;
}

uint32_t fvip_init_volume_index(uint32_t volume_id, fvip_table_t *out_table) {
    univip_tree_t *tree;

    if (!out_table) {
        return VIP_ERR_NULL_POINTER;
    }

    vip_memset(out_table, 0, sizeof(fvip_table_t));
    out_table->volume_id = volume_id;
    out_table->count = 0;
    out_table->initialized = true;

    tree = univip_get_tree(out_table);
    univip_tree_init(tree);

    return VIP_OK;
}

/* =========================================================================
 * UniVIP Volume-Level Navigation
 * ========================================================================= */

uint32_t univip_register_volume(uint32_t volume_id, uint64_t base_sector,
                                 const char *vol_label) {
    vip_volume_entry_t *slot;
    size_t label_len;
    uint32_t i;

    if (!g_vip_registry.initialized) {
        return VIP_ERR_SYSTEM_NOT_READY;
    }

    if (!vol_label) {
        return VIP_ERR_NULL_POINTER;
    }

    label_len = vip_strlen(vol_label);
    if (label_len >= VIP_LABEL_MAX) {
        return VIP_ERR_LABEL_TOO_LONG;
    }

    if (!fvip_str_is_sutf8(vol_label)) {
        return VIP_ERR_INVALID_SUTF8;
    }

    if (g_vip_registry.count >= VIP_MAX_VOLUMES) {
        return VIP_ERR_VOLUME_LIMIT;
    }

    /* Check for duplicate volume_id */
    for (i = 0; i < VIP_MAX_VOLUMES; ++i) {
        if (g_vip_registry.entries[i].registered &&
            g_vip_registry.entries[i].volume_id == volume_id) {
            return VIP_ERR_ALREADY_EXISTS;
        }
    }

    /* Find a free slot */
    slot = (void *)0;
    for (i = 0; i < VIP_MAX_VOLUMES; ++i) {
        if (!g_vip_registry.entries[i].registered) {
            slot = &g_vip_registry.entries[i];
            break;
        }
    }

    if (!slot) {
        return VIP_ERR_VOLUME_LIMIT;
    }

    slot->volume_id = volume_id;
    slot->base_sector = base_sector;
    vip_memcpy(slot->label, vol_label, label_len);
    slot->label[label_len] = '\0';
    slot->registered = true;
    ++g_vip_registry.count;

    return VIP_REGISTER_OK;
}

uint32_t univip_resolve_volume(uint32_t volume_id, uint64_t *out_base_sector) {
    uint32_t i;

    if (!out_base_sector) {
        return VIP_ERR_NULL_POINTER;
    }

    if (!g_vip_registry.initialized) {
        return VIP_ERR_SYSTEM_NOT_READY;
    }

    for (i = 0; i < VIP_MAX_VOLUMES; ++i) {
        if (g_vip_registry.entries[i].registered &&
            g_vip_registry.entries[i].volume_id == volume_id) {
            *out_base_sector = g_vip_registry.entries[i].base_sector;
            return VIP_RESOLVE_OK;
        }
    }

    return VIP_ERR_VOLUME_NOT_REGISTERED;
}

/* =========================================================================
 * Storage Flag Conversion (OWFS & USFS)
 * ========================================================================= */

uint32_t fvip_flags_from_storage_entry(uint8_t entry_type, uint32_t sec_flags) {
    uint32_t out = 0;

    if (entry_type & VIP_STORAGE_ENTRY_FILE) {
        out |= FVIP_FLAG_FILE;
    }
    if (entry_type & VIP_STORAGE_ENTRY_CATALOG) {
        out |= FVIP_FLAG_CATALOG;
    }
    if (entry_type & VIP_STORAGE_ENTRY_DELETED) {
        out |= FVIP_FLAG_DELETED;
    }
    if (sec_flags & VIP_STORAGE_SEC_ENCRYPTED) {
        out |= FVIP_FLAG_ENCRYPTED;
    }
    if (sec_flags & VIP_STORAGE_SEC_READONLY) {
        out |= FVIP_FLAG_READONLY;
    }
    if (sec_flags & VIP_STORAGE_SEC_HIDDEN) {
        out |= FVIP_FLAG_HIDDEN;
    }

    return out;
}

void fvip_flags_to_storage_entry(uint32_t fvip_flags,
                                 uint8_t *out_entry_type,
                                 uint32_t *out_sec_flags) {
    uint8_t entry_type = 0;
    uint32_t sec_flags = 0;

    if (fvip_flags & FVIP_FLAG_FILE) {
        entry_type |= VIP_STORAGE_ENTRY_FILE;
    }
    if (fvip_flags & FVIP_FLAG_CATALOG) {
        entry_type |= VIP_STORAGE_ENTRY_CATALOG;
    }
    /* Neither bit set defaults to FILE, matching filesystem semantics */
    if (entry_type == 0) {
        entry_type = VIP_STORAGE_ENTRY_FILE;
    }
    if (fvip_flags & FVIP_FLAG_DELETED) {
        entry_type |= VIP_STORAGE_ENTRY_DELETED;
    }

    if (fvip_flags & FVIP_FLAG_ENCRYPTED) {
        sec_flags |= VIP_STORAGE_SEC_ENCRYPTED;
    }
    if (fvip_flags & FVIP_FLAG_READONLY) {
        sec_flags |= VIP_STORAGE_SEC_READONLY;
    }
    if (fvip_flags & FVIP_FLAG_HIDDEN) {
        sec_flags |= VIP_STORAGE_SEC_HIDDEN;
    }

    if (out_entry_type) {
        *out_entry_type = entry_type;
    }
    if (out_sec_flags) {
        *out_sec_flags = sec_flags;
    }
}

/* =========================================================================
 * Absolute Addressing - OpenWindows-storage / MBL Integration
 * ========================================================================= */

static uint32_t fvip_volume_base_for_table(const fvip_table_t *table,
                                            uint64_t *out_base_lba) {
    uint32_t rc;

    if (!g_vip_registry.initialized) {
        return VIP_ERR_SYSTEM_NOT_READY;
    }

    rc = univip_resolve_volume(table->volume_id, out_base_lba);
    return rc;
}

uint32_t fvip_entry_absolute_byte(const fvip_table_t *table,
                                   const fvip_entry_t *entry,
                                   uint64_t *out_byte) {
    uint64_t base_lba = 0;
    uint32_t rc;

    if (!table || !entry || !out_byte) {
        return VIP_ERR_NULL_POINTER;
    }
    if (!table->initialized) {
        return VIP_ERR_NOT_INITIALIZED;
    }
    if (!entry->occupied) {
        return VIP_ERR_NOT_FOUND;
    }

    rc = fvip_volume_base_for_table(table, &base_lba);
    if (rc != VIP_RESOLVE_OK) {
        return rc;
    }

    *out_byte = (base_lba * VIP_SECTOR_SIZE) + entry->sector_offset;
    return VIP_OK;
}

uint32_t fvip_entry_absolute_lba(const fvip_table_t *table,
                                  const fvip_entry_t *entry,
                                  uint64_t *out_lba) {
    uint64_t base_lba = 0;
    uint32_t rc;

    if (!table || !entry || !out_lba) {
        return VIP_ERR_NULL_POINTER;
    }
    if (!table->initialized) {
        return VIP_ERR_NOT_INITIALIZED;
    }
    if (!entry->occupied) {
        return VIP_ERR_NOT_FOUND;
    }

    rc = fvip_volume_base_for_table(table, &base_lba);
    if (rc != VIP_RESOLVE_OK) {
        return rc;
    }

    *out_lba = base_lba + (entry->sector_offset / VIP_SECTOR_SIZE);
    return VIP_OK;
}

/* =========================================================================
 * FVIP File-Level Indexing - Insert
 * ========================================================================= */

uint32_t fvip_insert_entry(fvip_table_t *table, const char *path,
                            uint64_t sector_offset, uint32_t flags) {
    univip_tree_t *tree;
    fvip_entry_t *slot;
    uint64_t hash;
    uint32_t node_idx;
    size_t path_len;
    size_t cp_count = 0;
    uint32_t i;

    if (!table || !path) {
        return VIP_ERR_NULL_POINTER;
    }

    if (!table->initialized) {
        return VIP_ERR_NOT_INITIALIZED;
    }

    path_len = vip_strlen(path);
    if (path_len == 0 || path_len >= FVIP_PATH_MAX) {
        return VIP_ERR_PATH_TOO_LONG;
    }

    if (!fvip_str_is_sutf8(path)) {
        return VIP_ERR_INVALID_SUTF8;
    }

    if (fvip_str_codepoint_count(path, &cp_count) != VIP_OK) {
        return VIP_ERR_INVALID_SUTF8;
    }

    if (table->count >= FVIP_MAX_ENTRIES) {
        return VIP_ERR_TABLE_FULL;
    }

    /* Compute path hash */
    hash = fvip_hash_path(path, path_len);

    tree = univip_get_tree(table);

    /* Check for duplicate path via trie lookup */
    node_idx = univip_walk(tree, hash, false);
    if (node_idx != UNIVIP_NULL_NODE &&
        tree->nodes[node_idx].entry_index != UNIVIP_NULL_ENTRY) {
        return VIP_ERR_ALREADY_EXISTS;
    }

    /* Insert into trie */
    node_idx = univip_walk(tree, hash, true);
    if (node_idx == UNIVIP_NULL_NODE) {
        return VIP_BAN_POOL_EXHAUSTED;
    }

    /* Find a free entry slot */
    slot = (void *)0;
    for (i = 0; i < FVIP_MAX_ENTRIES; ++i) {
        if (!table->entries[i].occupied) {
            slot = &table->entries[i];
            break;
        }
    }

    if (!slot) {
        return VIP_ERR_TABLE_FULL;
    }

    /* Populate entry */
    vip_memcpy(slot->path, path, path_len);
    slot->path[path_len] = '\0';
    slot->sector_offset = sector_offset;
    slot->flags = flags;
    slot->codepoint_metadata = (uint32_t)cp_count;
    slot->created_epoch = 0;
    slot->modified_epoch = 0;
    slot->path_hash = hash;
    slot->occupied = true;

    /* Link trie node to entry */
    tree->nodes[node_idx].entry_index = i;
    ++tree->entry_count;
    ++table->count;

    return VIP_INSERT_OK;
}

/* =========================================================================
 * FVIP File-Level Indexing - Lookup
 * ========================================================================= */

uint32_t fvip_lookup_entry(const fvip_table_t *table, const char *path,
                            fvip_entry_t *out_entry) {
    const univip_tree_t *tree;
    uint64_t hash;
    uint32_t node_idx;
    uint32_t entry_idx;
    size_t path_len;

    if (!table || !path || !out_entry) {
        return VIP_ERR_NULL_POINTER;
    }

    if (!table->initialized) {
        return VIP_ERR_NOT_INITIALIZED;
    }

    path_len = vip_strlen(path);
    if (path_len == 0 || path_len >= FVIP_PATH_MAX) {
        return VIP_ERR_PATH_TOO_LONG;
    }

    hash = fvip_hash_path(path, path_len);

    tree = univip_get_tree_const(table);
    node_idx = univip_walk_const(tree, hash);

    if (node_idx == UNIVIP_NULL_NODE) {
        return VIP_ERR_NOT_FOUND;
    }

    entry_idx = tree->nodes[node_idx].entry_index;
    if (entry_idx >= FVIP_MAX_ENTRIES) {
        return VIP_BAN_ENTRY_CORRUPT;
    }

    /* Verify the entry is occupied and path matches */
    if (!table->entries[entry_idx].occupied) {
        return VIP_BAN_ENTRY_CORRUPT;
    }

    vip_memcpy(out_entry, &table->entries[entry_idx], sizeof(fvip_entry_t));
    return VIP_LOOKUP_OK;
}

/* =========================================================================
 * FVIP File-Level Indexing - Remove
 * ========================================================================= */

uint32_t fvip_remove_entry(fvip_table_t *table, const char *path) {
    univip_tree_t *tree;
    uint64_t hash;
    uint32_t node_idx;
    uint32_t entry_idx;
    size_t path_len;

    if (!table || !path) {
        return VIP_ERR_NULL_POINTER;
    }

    if (!table->initialized) {
        return VIP_ERR_NOT_INITIALIZED;
    }

    path_len = vip_strlen(path);
    if (path_len == 0 || path_len >= FVIP_PATH_MAX) {
        return VIP_ERR_PATH_TOO_LONG;
    }

    hash = fvip_hash_path(path, path_len);

    tree = univip_get_tree(table);
    node_idx = univip_walk(tree, hash, false);

    if (node_idx == UNIVIP_NULL_NODE) {
        return VIP_ERR_NOT_FOUND;
    }

    entry_idx = tree->nodes[node_idx].entry_index;
    if (entry_idx >= FVIP_MAX_ENTRIES) {
        return VIP_BAN_ENTRY_CORRUPT;
    }

    if (!table->entries[entry_idx].occupied) {
        return VIP_ERR_NOT_FOUND;
    }

    /* Clear entry */
    vip_memset(&table->entries[entry_idx], 0, sizeof(fvip_entry_t));
    table->entries[entry_idx].occupied = false;

    /* Disconnect trie node */
    tree->nodes[node_idx].entry_index = UNIVIP_NULL_ENTRY;
    --tree->entry_count;
    --table->count;

    /* Prune empty trie branches */
    univip_prune(tree, hash);

    return VIP_REMOVE_OK;
}

/* =========================================================================
 * FVIP Diagnostics - Integrity Verification
 * ========================================================================= */

uint32_t fvip_verify_integrity(const fvip_table_t *table) {
    const univip_tree_t *tree;
    uint32_t occupied_count = 0;
    uint32_t trie_terminals = 0;
    uint32_t i;
    uint32_t level;

    if (!table) {
        return VIP_ERR_NULL_POINTER;
    }

    if (!table->initialized) {
        return VIP_ERR_NOT_INITIALIZED;
    }

    tree = univip_get_tree_const(table);

    /* Phase 1: Count occupied entries and verify hash consistency */
    for (i = 0; i < FVIP_MAX_ENTRIES; ++i) {
        if (table->entries[i].occupied) {
            size_t plen;
            size_t cp_count;
            uint64_t expected_hash;

            ++occupied_count;

            plen = vip_strlen(table->entries[i].path);
            if (plen == 0 || plen >= FVIP_PATH_MAX) {
                return VIP_BAN_ENTRY_CORRUPT;
            }

            if (!fvip_str_is_sutf8(table->entries[i].path)) {
                return VIP_BAN_ENTRY_CORRUPT;
            }

            if (fvip_str_codepoint_count(table->entries[i].path, &cp_count) != VIP_OK ||
                table->entries[i].codepoint_metadata != (uint32_t)cp_count) {
                return VIP_BAN_ENTRY_CORRUPT;
            }

            expected_hash = fvip_hash_path(table->entries[i].path, plen);
            if (table->entries[i].path_hash != expected_hash) {
                return VIP_BAN_ENTRY_CORRUPT;
            }
        }
    }

    /* Verify count matches */
    if (occupied_count != table->count) {
        return VIP_BAN_TABLE_CORRUPT;
    }

    /* Phase 2: Walk trie and count terminal nodes */
    if (tree->root == UNIVIP_NULL_NODE) {
        if (occupied_count != 0) {
            return VIP_BAN_TABLE_CORRUPT;
        }
        return VIP_INTEGRITY_OK;
    }

    /* Iterative DFS over the trie to count terminals */
    for (i = 0; i < UNIVIP_MAX_NODES; ++i) {
        if (tree->nodes[i].entry_index != UNIVIP_NULL_ENTRY &&
            tree->nodes[i].entry_index < FVIP_MAX_ENTRIES) {
            if (table->entries[tree->nodes[i].entry_index].occupied) {
                ++trie_terminals;
            }
        }
        /* Validate child pointers */
        for (level = 0; level < UNIVIP_CHILDREN_PER_NODE; ++level) {
            if (tree->nodes[i].child_bitmap & ((uint16_t)1U << level)) {
                if (tree->nodes[i].children[level] >= UNIVIP_MAX_NODES) {
                    return VIP_BAN_NODE_CORRUPT;
                }
            }
        }
    }

    if (trie_terminals != table->count) {
        return VIP_BAN_TABLE_CORRUPT;
    }

    return VIP_INTEGRITY_OK;
}
