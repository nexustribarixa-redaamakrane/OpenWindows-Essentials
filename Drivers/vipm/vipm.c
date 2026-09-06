/*
 * vipm.c - Volume Index Partition Manager (.owc)
 *
 * Sparse 44-bit hex trie (UniVIP) + FVIP path-table driver. Zero dynamic
 * allocation: node pool is a fixed slab inside the manager context.
 * Every public routine speaks BANcode. Paths/labels are SUTF-8.
 */
#include "vipm.h"
#include "sutf8.h"

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static unsigned vp_popcount16(unsigned v)
{
    unsigned c = 0u;
    while (v != 0u) {
        c += v & 1u;
        v >>= 1;
    }
    return c;
}

static uint16_t vp_node_alloc(vipm_manager_t *mgr)
{
    if (mgr->free_top == 0u) {
        return VIPM_NULL_INDEX;
    }
    mgr->free_top--;
    uint16_t idx = mgr->free_stack[mgr->free_top];
    return idx;
}

static void vp_node_free(vipm_manager_t *mgr, uint16_t idx)
{
    mgr->free_stack[mgr->free_top] = idx;
    mgr->free_top++;
}

/* Sparse child lookup: children[] kept sorted by nibble index. */
static uint16_t vp_node_get(const vipm_node_t *n, unsigned nib)
{
    uint16_t bit = (uint16_t)(1u << nib);
    if ((n->child_bitmap & bit) == 0u) {
        return VIPM_NULL_INDEX;
    }
    unsigned pos = vp_popcount16((unsigned)(n->child_bitmap &
                                            (uint16_t)(bit - 1u)));
    return n->children[pos];
}

static void vp_node_set(vipm_node_t *n, unsigned nib, uint16_t idx)
{
    uint16_t bit = (uint16_t)(1u << nib);
    unsigned pos = vp_popcount16((unsigned)(n->child_bitmap &
                                            (uint16_t)(bit - 1u)));
    if ((n->child_bitmap & bit) == 0u) {
        unsigned i = vp_popcount16((unsigned)n->child_bitmap);
        while (i > pos) {
            n->children[i] = n->children[i - 1u];
            i--;
        }
        n->children[pos] = idx;
        n->child_bitmap = (uint16_t)(n->child_bitmap | bit);
    } else {
        n->children[pos] = idx;
    }
}

static void vp_node_unset(vipm_node_t *n, unsigned nib)
{
    uint16_t bit = (uint16_t)(1u << nib);
    if ((n->child_bitmap & bit) == 0u) {
        return;
    }
    unsigned pos = vp_popcount16((unsigned)(n->child_bitmap &
                                            (uint16_t)(bit - 1u)));
    unsigned end = vp_popcount16((unsigned)n->child_bitmap);
    unsigned i = pos;
    while (i + 1u < end) {
        n->children[i] = n->children[i + 1u];
        i++;
    }
    n->children[end - 1u] = VIPM_NULL_INDEX;
    n->child_bitmap = (uint16_t)(n->child_bitmap & (uint16_t)~bit);
}

/* depth is 0..10 (nibble index from most significant). */
static unsigned vp_nibble(uint64_t key, unsigned depth)
{
    unsigned shift = (VIPM_NIBBLES - 1u - depth) * 4u;
    return (unsigned)((key >> shift) & 0xFu);
}

static uint64_t vp_fnv1a64(const char *s)
{
    uint64_t h = 0xcbf29ce484222325ULL;
    while (*s != 0) {
        h ^= (uint64_t)(uint8_t)*s;
        h *= 0x100000001b3ULL;
        s++;
    }
    return h;
}

static bool vp_path_equal(const char *a, const char *b)
{
    size_t i = 0u;
    for (;;) {
        if (a[i] != b[i]) {
            return false;
        }
        if (a[i] == 0) {
            return true;
        }
        i++;
    }
}

static vipm_status_t vp_validate_path(const char *path, size_t *out_len,
                                      uint32_t *out_codepoints)
{
    size_t blen = 0u;
    while (path[blen] != 0) {
        blen++;
    }
    if (blen >= VIPM_PATH_MAX) {
        return VIPM_ERR_PATH_TOO_LONG;
    }
    uint32_t cpcount = 0u;
    size_t pos = 0u;
    while (pos < blen) {
        sucs_char_t cp;
        size_t adv = sutf8_next_codepoint((const uint8_t *)(path + pos),
                                          blen - pos, &cp);
        if (adv == 0u) {
            break;
        }
        if (cp == SUCS_INVALID_CODEPOINT) {
            return VIPM_ERR_INVALID_SUTF8;
        }
        pos += adv;
        cpcount++;
    }
    *out_len = blen;
    *out_codepoints = cpcount;
    return VIPM_OK;
}

/* Const trie walk (no telemetry side effects) used by integrity checks. */
static vipm_status_t vp_trie_find(const vipm_manager_t *mgr, uint64_t key,
                                  uint32_t *out_entry_index)
{
    if (!vipm_key_valid(key)) {
        return VIPM_ERR_INVALID_KEY;
    }
    uint16_t cur = mgr->root;
    unsigned d;
    for (d = 0u; d < VIPM_NIBBLES; d++) {
        unsigned nb = vp_nibble(key, d);
        uint16_t nxt = vp_node_get(&mgr->nodes[cur], nb);
        if (nxt == VIPM_NULL_INDEX) {
            return VIPM_ERR_NOT_FOUND;
        }
        cur = nxt;
    }
    if (mgr->nodes[cur].payload == VIPM_NULL_PAYLOAD) {
        return VIPM_ERR_NOT_FOUND;
    }
    *out_entry_index = mgr->nodes[cur].payload;
    return VIPM_OK;
}

/* ------------------------------------------------------------------ */
/*  Key geometry                                                       */
/* ------------------------------------------------------------------ */

uint64_t vipm_key_make(uint8_t volume_id, uint64_t byte_offset)
{
    uint64_t top = ((uint64_t)(volume_id & 0x0Fu)) << 40u;
    return top | (byte_offset & 0x000000FFFFFFFFFFULL);
}

bool vipm_key_valid(uint64_t key)
{
    return (key >> VIPM_KEY_BITS) == 0u;
}

/* ------------------------------------------------------------------ */
/*  Manager lifecycle                                                  */
/* ------------------------------------------------------------------ */

vipm_status_t vipm_init_manager(vipm_manager_t *mgr)
{
    if (mgr == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    size_t i;
    for (i = 0u; i < VIPM_MAX_NODES; i++) {
        mgr->nodes[i].child_bitmap = 0u;
        mgr->nodes[i].payload = VIPM_NULL_PAYLOAD;
        for (size_t c = 0u; c < VIPM_CHILDREN; c++) {
            mgr->nodes[i].children[c] = VIPM_NULL_INDEX;
        }
        mgr->free_stack[i] = (uint16_t)i;
    }
    mgr->free_top = (uint16_t)VIPM_MAX_NODES;
    mgr->node_count = 0u;
    mgr->volume_count = 0u;
    mgr->entry_count = 0u;
    for (i = 0u; i < VIPM_MAX_VOLUMES; i++) {
        mgr->volumes[i].registered = false;
    }
    for (i = 0u; i < VIPM_FVIP_MAX_ENTRIES; i++) {
        mgr->entries[i].occupied = false;
    }
    mgr->trie_lookups = 0u;
    mgr->trie_inserts = 0u;
    mgr->trie_removes = 0u;
    mgr->trie_max_depth = 0u;
    mgr->fvip_hits = 0u;
    mgr->fvip_misses = 0u;

    mgr->root = vp_node_alloc(mgr);
    if (mgr->root == VIPM_NULL_INDEX) {
        return VIPM_BAN_POOL_EXHAUSTED;
    }
    mgr->node_count = 1u;
    mgr->initialized = true;
    return VIPM_OK;
}

/* ------------------------------------------------------------------ */
/*  Volume registry                                                    */
/* ------------------------------------------------------------------ */

vipm_status_t vipm_volume_register(vipm_manager_t *mgr, uint8_t volume_id,
                                   uint64_t base_sector, const char *label)
{
    if (mgr == NULL || label == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }
    if (volume_id >= VIPM_MAX_VOLUMES) {
        return VIPM_ERR_INVALID_KEY;
    }

    size_t blen;
    uint32_t cpcount;
    vipm_status_t st = vp_validate_path(label, &blen, &cpcount);
    if (st != VIPM_OK) {
        return VIPM_ERR_LABEL_TOO_LONG;
    }

    size_t slot = VIPM_NULL_INDEX;
    for (size_t i = 0u; i < VIPM_MAX_VOLUMES; i++) {
        if (mgr->volumes[i].registered) {
            if (mgr->volumes[i].volume_id == volume_id) {
                return VIPM_BAN_VOLUME_CORRUPT;
            }
        } else if (slot == VIPM_NULL_INDEX) {
            slot = i;
        }
    }
    if (slot == VIPM_NULL_INDEX) {
        return VIPM_ERR_VOLUME_LIMIT;
    }

    size_t i;
    for (i = 0u; i < blen; i++) {
        mgr->volumes[slot].label[i] = label[i];
    }
    mgr->volumes[slot].label[blen] = 0;
    mgr->volumes[slot].label_codepoints = cpcount;
    mgr->volumes[slot].volume_id = volume_id;
    mgr->volumes[slot].base_sector = base_sector;
    mgr->volumes[slot].registered = true;
    mgr->volume_count++;
    return VIPM_OK;
}

vipm_status_t vipm_volume_resolve(const vipm_manager_t *mgr,
                                  uint8_t volume_id,
                                  uint64_t *out_base_sector)
{
    if (mgr == NULL || out_base_sector == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }
    for (size_t i = 0u; i < VIPM_MAX_VOLUMES; i++) {
        if (mgr->volumes[i].registered &&
            mgr->volumes[i].volume_id == volume_id) {
            *out_base_sector = mgr->volumes[i].base_sector;
            return VIPM_OK;
        }
    }
    return VIPM_ERR_NOT_FOUND;
}

/* ------------------------------------------------------------------ */
/*  Trie mapping ops                                                   */
/* ------------------------------------------------------------------ */

vipm_status_t vipm_mapping_put(vipm_manager_t *mgr, uint64_t key,
                               uint32_t entry_index)
{
    if (mgr == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }
    if (!vipm_key_valid(key)) {
        return VIPM_ERR_INVALID_KEY;
    }
    if (entry_index >= VIPM_FVIP_MAX_ENTRIES) {
        return VIPM_ERR_INVALID_KEY;
    }

    /* Pre-count nodes needed along a missing path so B+ exhaustion is
       atomic (the trie is never left half-written). */
    uint16_t cur = mgr->root;
    uint32_t missing = 0u;
    unsigned d;
    for (d = 0u; d < VIPM_NIBBLES; d++) {
        unsigned nb = vp_nibble(key, d);
        uint16_t nxt = vp_node_get(&mgr->nodes[cur], nb);
        if (nxt == VIPM_NULL_INDEX) {
            missing = VIPM_NIBBLES - d;
            break;
        }
        cur = nxt;
    }
    if (missing > (uint32_t)mgr->free_top) {
        return VIPM_BAN_POOL_EXHAUSTED;
    }

    cur = mgr->root;
    for (d = 0u; d < VIPM_NIBBLES; d++) {
        unsigned nb = vp_nibble(key, d);
        uint16_t nxt = vp_node_get(&mgr->nodes[cur], nb);
        if (nxt == VIPM_NULL_INDEX) {
            uint16_t nn = vp_node_alloc(mgr);
            if (nn == VIPM_NULL_INDEX) {
                return VIPM_BAN_POOL_EXHAUSTED;
            }
            mgr->nodes[nn].child_bitmap = 0u;
            mgr->nodes[nn].payload = VIPM_NULL_PAYLOAD;
            for (size_t c = 0u; c < VIPM_CHILDREN; c++) {
                mgr->nodes[nn].children[c] = VIPM_NULL_INDEX;
            }
            vp_node_set(&mgr->nodes[cur], nb, nn);
            mgr->node_count++;
            cur = nn;
        } else {
            cur = nxt;
        }
    }

    bool had_payload = (mgr->nodes[cur].payload != VIPM_NULL_PAYLOAD);
    mgr->nodes[cur].payload = entry_index;
    mgr->trie_inserts++;
    return had_payload ? VIPM_ERR_DUPLICATE_MAPPING : VIPM_OK;
}

vipm_status_t vipm_mapping_get(vipm_manager_t *mgr, uint64_t key,
                               uint32_t *out_entry_index)
{
    if (mgr == NULL || out_entry_index == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }
    mgr->trie_lookups++;
    return vp_trie_find(mgr, key, out_entry_index);
}

vipm_status_t vipm_mapping_remove(vipm_manager_t *mgr, uint64_t key)
{
    if (mgr == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }
    if (!vipm_key_valid(key)) {
        return VIPM_ERR_INVALID_KEY;
    }

    uint16_t parents[VIPM_NIBBLES];
    unsigned pnibbles[VIPM_NIBBLES];
    uint16_t cur = mgr->root;
    unsigned d;
    for (d = 0u; d < VIPM_NIBBLES; d++) {
        unsigned nb = vp_nibble(key, d);
        parents[d] = cur;
        pnibbles[d] = nb;
        uint16_t nxt = vp_node_get(&mgr->nodes[cur], nb);
        if (nxt == VIPM_NULL_INDEX) {
            return VIPM_ERR_NOT_FOUND;
        }
        cur = nxt;
    }
    if (mgr->nodes[cur].payload == VIPM_NULL_PAYLOAD) {
        return VIPM_ERR_NOT_FOUND;
    }

    mgr->nodes[cur].payload = VIPM_NULL_PAYLOAD;
    mgr->trie_removes++;

    /* Prune leaf-up: any empty, childless node returns to the slab. */
    unsigned depth = VIPM_NIBBLES;
    while (depth > 0u) {
        uint16_t node = cur;
        uint16_t parent = parents[depth - 1u];
        unsigned nib = pnibbles[depth - 1u];
        if (mgr->nodes[node].child_bitmap == 0u &&
            mgr->nodes[node].payload == VIPM_NULL_PAYLOAD) {
            vp_node_unset(&mgr->nodes[parent], nib);
            mgr->nodes[node].child_bitmap = 0u;
            if (mgr->node_count > 0u) {
                mgr->node_count--;
            }
            vp_node_free(mgr, node);
        }
        cur = parent;
        depth--;
    }
    return VIPM_OK;
}

/* ------------------------------------------------------------------ */
/*  FVIP path table                                                    */
/* ------------------------------------------------------------------ */

vipm_status_t vipm_fvip_insert(vipm_manager_t *mgr, uint8_t volume_id,
                               const char *path, uint64_t byte_offset,
                               uint32_t flags)
{
    if (mgr == NULL || path == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }
    if (!vipm_key_valid(vipm_key_make(volume_id, byte_offset))) {
        return VIPM_ERR_INVALID_KEY;
    }

    bool vol_found = false;
    for (size_t i = 0u; i < VIPM_MAX_VOLUMES; i++) {
        if (mgr->volumes[i].registered &&
            mgr->volumes[i].volume_id == volume_id) {
            vol_found = true;
            break;
        }
    }
    if (!vol_found) {
        return VIPM_ERR_NOT_FOUND;
    }

    /* Path / offset collision guards BEFORE any mutation. */
    for (size_t i = 0u; i < VIPM_FVIP_MAX_ENTRIES; i++) {
        if (!mgr->entries[i].occupied ||
            mgr->entries[i].volume_id != volume_id) {
            continue;
        }
        if (mgr->entries[i].byte_offset != byte_offset) {
            continue;
        }
        if (vp_path_equal(mgr->entries[i].path, path)) {
            return VIPM_ERR_ALREADY_EXISTS;
        }
        return VIPM_ERR_DUPLICATE_MAPPING;
    }

    size_t blen;
    uint32_t cpcount;
    vipm_status_t st = vp_validate_path(path, &blen, &cpcount);
    if (st != VIPM_OK) {
        return st;
    }

    uint16_t slot = VIPM_NULL_INDEX;
    for (size_t i = 0u; i < VIPM_FVIP_MAX_ENTRIES; i++) {
        if (!mgr->entries[i].occupied) {
            slot = (uint16_t)i;
            break;
        }
    }
    if (slot == VIPM_NULL_INDEX) {
        return VIPM_ERR_TABLE_FULL;
    }

    size_t i;
    for (i = 0u; i < blen; i++) {
        mgr->entries[slot].path[i] = path[i];
    }
    for (i = blen; i < VIPM_PATH_MAX; i++) {
        mgr->entries[slot].path[i] = 0;
    }
    mgr->entries[slot].byte_offset = byte_offset;
    mgr->entries[slot].flags = flags;
    mgr->entries[slot].codepoint_meta = cpcount;
    mgr->entries[slot].path_hash = vp_fnv1a64(path);
    mgr->entries[slot].volume_id = volume_id;
    mgr->entries[slot].occupied = true;
    mgr->entry_count++;

    uint64_t key = vipm_key_make(volume_id, byte_offset);
    st = vipm_mapping_put(mgr, key, (uint32_t)slot);
    if (st != VIPM_OK) {
        mgr->entries[slot].occupied = false;
        mgr->entry_count--;
        return st;
    }
    return VIPM_OK;
}

vipm_status_t vipm_fvip_lookup(vipm_manager_t *mgr, uint8_t volume_id,
                               const char *path,
                               vipm_fvip_entry_t *out_entry)
{
    if (mgr == NULL || path == NULL || out_entry == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }

    for (size_t i = 0u; i < VIPM_FVIP_MAX_ENTRIES; i++) {
        if (!mgr->entries[i].occupied ||
            mgr->entries[i].volume_id != volume_id) {
            continue;
        }
        if (!vp_path_equal(mgr->entries[i].path, path)) {
            continue;
        }
        if (mgr->entries[i].path_hash != vp_fnv1a64(path)) {
            return VIPM_BAN_HASH_MISMATCH;
        }
        size_t blen;
        uint32_t cps;
        vipm_status_t st = vp_validate_path(path, &blen, &cps);
        if (st != VIPM_OK) {
            return st;
        }
        if (mgr->entries[i].codepoint_meta != cps) {
            return VIPM_BAN_ENTRY_CORRUPT;
        }
        uint32_t trie_idx;
        uint64_t key = vipm_key_make(volume_id, mgr->entries[i].byte_offset);
        st = vp_trie_find(mgr, key, &trie_idx);
        if (st != VIPM_OK || trie_idx != (uint32_t)i) {
            return VIPM_ERR_INDEX_STALE;
        }
        *out_entry = mgr->entries[i];
        mgr->fvip_hits++;
        return VIPM_OK;
    }
    mgr->fvip_misses++;
    return VIPM_ERR_NOT_FOUND;
}

vipm_status_t vipm_fvip_remove(vipm_manager_t *mgr, uint8_t volume_id,
                               const char *path)
{
    if (mgr == NULL || path == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }

    for (size_t i = 0u; i < VIPM_FVIP_MAX_ENTRIES; i++) {
        if (!mgr->entries[i].occupied ||
            mgr->entries[i].volume_id != volume_id) {
            continue;
        }
        if (!vp_path_equal(mgr->entries[i].path, path)) {
            continue;
        }
        uint64_t key = vipm_key_make(volume_id, mgr->entries[i].byte_offset);
        vipm_status_t st = vipm_mapping_remove(mgr, key);
        mgr->entries[i].occupied = false;
        mgr->entry_count--;
        for (size_t b = 0u; b < VIPM_PATH_MAX; b++) {
            mgr->entries[i].path[b] = 0;
        }
        mgr->entries[i].byte_offset = 0u;
        mgr->entries[i].flags = 0u;
        mgr->entries[i].codepoint_meta = 0u;
        mgr->entries[i].path_hash = 0u;
        mgr->entries[i].volume_id = 0u;
        if (st != VIPM_OK) {
            return st;
        }
        return VIPM_OK;
    }
    return VIPM_ERR_NOT_FOUND;
}

/* ------------------------------------------------------------------ */
/*  Storage flag mapping (OWFS/USFS shared on-disk layout)             */
/* ------------------------------------------------------------------ */

uint32_t vipm_flags_from_storage(uint8_t entry_type, uint32_t sec_flags)
{
    uint32_t f = 0u;
    if (entry_type == VIPM_ENTRY_CATALOG) {
        f |= VIPM_FLAG_CATALOG;
    } else {
        f |= VIPM_FLAG_FILE;
    }
    if ((entry_type & VIPM_ENTRY_DELETED) != 0u) {
        f |= VIPM_FLAG_DELETED;
    }
    if ((sec_flags & 0x0001u) != 0u) {
        f |= VIPM_FLAG_HIDDEN;
    }
    if ((sec_flags & 0x0002u) != 0u) {
        f |= VIPM_FLAG_READONLY;
    }
    if ((sec_flags & 0x0004u) != 0u) {
        f |= VIPM_FLAG_ENCRYPTED;
    }
    if ((sec_flags & 0x0008u) != 0u) {
        f |= VIPM_FLAG_SYSTEM;
    }
    return f;
}

void vipm_flags_to_storage(uint32_t fvip_flags, uint8_t *out_entry_type,
                           uint32_t *out_sec_flags)
{
    if (out_entry_type == NULL || out_sec_flags == NULL) {
        return;
    }
    uint8_t etype = VIPM_ENTRY_FILE;
    uint32_t sec = 0u;
    if ((fvip_flags & VIPM_FLAG_CATALOG) != 0u) {
        etype = VIPM_ENTRY_CATALOG;
    }
    if ((fvip_flags & VIPM_FLAG_DELETED) != 0u) {
        etype = (uint8_t)(etype | VIPM_ENTRY_DELETED);
    }
    if ((fvip_flags & VIPM_FLAG_HIDDEN) != 0u) {
        sec |= 0x0001u;
    }
    if ((fvip_flags & VIPM_FLAG_READONLY) != 0u) {
        sec |= 0x0002u;
    }
    if ((fvip_flags & VIPM_FLAG_ENCRYPTED) != 0u) {
        sec |= 0x0004u;
    }
    if ((fvip_flags & VIPM_FLAG_SYSTEM) != 0u) {
        sec |= 0x0008u;
    }
    *out_entry_type = etype;
    *out_sec_flags = sec;
}

/* ------------------------------------------------------------------ */
/*  Integrity verification                                             */
/* ------------------------------------------------------------------ */

vipm_status_t vipm_integrity_verify(const vipm_manager_t *mgr)
{
    if (mgr == NULL) {
        return VIPM_ERR_NULL_POINTER;
    }
    if (!mgr->initialized) {
        return VIPM_ERR_NOT_INIT;
    }

    uint8_t visited[VIPM_MAX_NODES];
    uint8_t in_free[VIPM_MAX_NODES];
    for (size_t i = 0u; i < VIPM_MAX_NODES; i++) {
        visited[i] = 0u;
        in_free[i] = 0u;
    }

    /* DFS over reachable nodes; detect cycles / dup children. */
    uint16_t stack[VIPM_MAX_NODES];
    uint16_t sp = 0u;
    stack[sp++] = mgr->root;
    visited[mgr->root] = 1u;
    uint32_t reachable = 0u;

    while (sp > 0u) {
        uint16_t cur = stack[--sp];
        reachable++;
        const vipm_node_t *n = &mgr->nodes[cur];

        unsigned bits = vp_popcount16((unsigned)n->child_bitmap);
        if (bits > VIPM_CHILDREN) {
            return VIPM_BAN_NODE_CORRUPT;
        }
        for (unsigned nb = 0u; nb < VIPM_CHILDREN; nb++) {
            uint16_t bit = (uint16_t)(1u << nb);
            if ((n->child_bitmap & bit) == 0u) {
                continue;
            }
            uint16_t cidx = vp_node_get(n, nb);
            if (cidx == VIPM_NULL_INDEX || cidx >= VIPM_MAX_NODES) {
                return VIPM_BAN_NODE_CORRUPT;
            }
            if (visited[cidx] != 0u) {
                return VIPM_BAN_NODE_CORRUPT;
            }
            visited[cidx] = 1u;
            if (sp >= VIPM_MAX_NODES) {
                return VIPM_BAN_NODE_CORRUPT;
            }
            stack[sp++] = cidx;
        }

        if (n->payload != VIPM_NULL_PAYLOAD) {
            if (n->payload >= VIPM_FVIP_MAX_ENTRIES) {
                return VIPM_BAN_NODE_CORRUPT;
            }
            const vipm_fvip_entry_t *e = &mgr->entries[n->payload];
            if (!e->occupied) {
                return VIPM_BAN_ENTRY_CORRUPT;
            }
            uint64_t key = vipm_key_make(e->volume_id, e->byte_offset);
            if (!vipm_key_valid(key)) {
                return VIPM_BAN_ENTRY_CORRUPT;
            }
            if (e->path_hash != vp_fnv1a64(e->path)) {
                return VIPM_BAN_HASH_MISMATCH;
            }
        }
    }

    /* Freelist sanity: indices in range, unique, never allocated. */
    for (size_t i = 0u; i < mgr->free_top; i++) {
        uint16_t fi = mgr->free_stack[i];
        if (fi >= VIPM_MAX_NODES) {
            return VIPM_BAN_TABLE_CORRUPT;
        }
        if (visited[fi] != 0u) {
            return VIPM_BAN_TABLE_CORRUPT;
        }
        if (in_free[fi] != 0u) {
            return VIPM_BAN_TABLE_CORRUPT;
        }
        in_free[fi] = 1u;
    }

    if (reachable != (uint32_t)mgr->node_count) {
        return VIPM_BAN_TABLE_CORRUPT;
    }

    /* Entry table: occupied count coherent, every entry mapped back. */
    uint32_t occupied = 0u;
    for (size_t i = 0u; i < VIPM_FVIP_MAX_ENTRIES; i++) {
        if (!mgr->entries[i].occupied) {
            continue;
        }
        occupied++;
        uint32_t trie_idx;
        uint64_t key = vipm_key_make(mgr->entries[i].volume_id,
                                     mgr->entries[i].byte_offset);
        vipm_status_t st = vp_trie_find(mgr, key, &trie_idx);
        if (st != VIPM_OK) {
            return VIPM_BAN_ENTRY_CORRUPT;
        }
        if (trie_idx != (uint32_t)i) {
            return VIPM_ERR_INDEX_STALE;
        }
        if (mgr->entries[i].path_hash != vp_fnv1a64(mgr->entries[i].path)) {
            return VIPM_BAN_HASH_MISMATCH;
        }
    }
    if (occupied != (uint32_t)mgr->entry_count) {
        return VIPM_BAN_ENTRY_CORRUPT;
    }

    /* Volume registry: no duplicate volume ids. */
    for (size_t i = 0u; i < VIPM_MAX_VOLUMES; i++) {
        if (!mgr->volumes[i].registered) {
            continue;
        }
        for (size_t j = i + 1u; j < VIPM_MAX_VOLUMES; j++) {
            if (mgr->volumes[j].registered &&
                mgr->volumes[i].volume_id == mgr->volumes[j].volume_id) {
                return VIPM_BAN_VOLUME_CORRUPT;
            }
        }
    }

    return VIPM_OK;
}

/* ------------------------------------------------------------------ */
/*  Telemetry                                                          */
/* ------------------------------------------------------------------ */

void vipm_get_telemetry(const vipm_manager_t *mgr,
                        uint64_t *out_trie_lookups,
                        uint64_t *out_trie_inserts,
                        uint64_t *out_trie_depth,
                        uint64_t *out_fvip_hits,
                        uint64_t *out_fvip_misses)
{
    if (mgr == NULL) {
        return;
    }
    if (out_trie_lookups != NULL) {
        *out_trie_lookups = mgr->trie_lookups;
    }
    if (out_trie_inserts != NULL) {
        *out_trie_inserts = mgr->trie_inserts;
    }
    if (out_trie_depth != NULL) {
        *out_trie_depth = mgr->trie_max_depth;
    }
    if (out_fvip_hits != NULL) {
        *out_fvip_hits = mgr->fvip_hits;
    }
    if (out_fvip_misses != NULL) {
        *out_fvip_misses = mgr->fvip_misses;
    }
}