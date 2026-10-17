/*
 * owxml64.c - OpenWindows Minimal XML Parser (.owd)
 *
 * Single-pass, non-validating XML reader over fixed static pools.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owxml64.h"

/* ── Internal state ───────────────────────────────────────────── */

static owxml_node_t  g_xml_nodes[OWXML_MAX_NODES];
static owxml_attr_t  g_xml_attrs[OWXML_MAX_ATTRS];
static char          g_xml_strings[OWXML_STRING_POOL];
static owxml_index_t g_xml_root = OWXML_NONE;
static bool          owxml64_ready = false;

typedef struct {
    const char   *text;
    size_t        len;
    size_t        pos;
    owxml_index_t node_count;
    owxml_index_t attr_count;
    uint32_t      str_len;
    owxml_index_t root;
    bool          error;
    owxml_index_t stack[OWXML_MAX_DEPTH];
    uint32_t      depth;
} ow_xml_parser_t;

/* ── Character helpers ────────────────────────────────────────── */

static bool xml_isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static bool xml_is_name_char(int c)
{
    if (c < 0) {
        return false;
    }
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) {
        return true;
    }
    return (c == '_' || c == '-' || c == ':' || c == '.' || c >= 0x80);
}

static int xml_peek(const ow_xml_parser_t *p)
{
    if (p->pos >= p->len) {
        return -1;
    }
    return (int)(unsigned char)p->text[p->pos];
}

static void xml_skip_ws(ow_xml_parser_t *p)
{
    while (p->pos < p->len && xml_isspace(xml_peek(p))) {
        p->pos++;
    }
}

/* True (and advance) when `p` is positioned at the literal `s`. */
static bool xml_match(ow_xml_parser_t *p, const char *s)
{
    size_t i = 0u;

    while (s[i] != '\0') {
        if (p->pos + i >= p->len || p->text[p->pos + i] != s[i]) {
            return false;
        }
        i++;
    }
    p->pos += i;
    return true;
}

/* ── Pools ────────────────────────────────────────────────────── */

static bool xml_pool_put(owxml_index_t base, uint32_t *w, uint8_t ch)
{
    if ((uint32_t)base + *w + 2u > (uint32_t)OWXML_STRING_POOL) {
        return false;
    }
    g_xml_strings[base + *w] = (char)ch;
    (*w)++;
    return true;
}

static bool xml_pool_streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }
    return (*a == *b);
}

static owxml_index_t xml_alloc_node(ow_xml_parser_t *p)
{
    owxml_node_t *n;

    if (p->node_count >= (owxml_index_t)OWXML_MAX_NODES) {
        p->error = true;
        return OWXML_NONE;
    }
    n = &g_xml_nodes[p->node_count];
    n->tag_off      = OWXML_NONE;
    n->text_off     = OWXML_NONE;
    n->first_attr   = OWXML_NONE;
    n->first_child  = OWXML_NONE;
    n->next_sibling = OWXML_NONE;
    n->parent       = OWXML_NONE;
    n->text_len     = 0u;
    return p->node_count++;
}

static void xml_add_attr(ow_xml_parser_t *p, owxml_index_t node,
                         owxml_index_t name, owxml_index_t value)
{
    owxml_index_t a;

    if (p->attr_count >= (owxml_index_t)OWXML_MAX_ATTRS) {
        p->error = true;
        return;
    }
    a = p->attr_count++;
    g_xml_attrs[a].name_off  = name;
    g_xml_attrs[a].value_off = value;
    g_xml_attrs[a].next      = OWXML_NONE;

    if (g_xml_nodes[node].first_attr == OWXML_NONE) {
        g_xml_nodes[node].first_attr = a;
    } else {
        owxml_index_t cur = g_xml_nodes[node].first_attr;
        while (g_xml_attrs[cur].next != OWXML_NONE) {
            cur = g_xml_attrs[cur].next;
        }
        g_xml_attrs[cur].next = a;
    }
}

/* Append `run_len` decoded bytes to an element's text accumulator. */
static bool xml_append_run(ow_xml_parser_t *p, owxml_index_t node,
                           const char *run, uint32_t run_len)
{
    owxml_node_t *n = &g_xml_nodes[node];
    uint32_t old_len = (n->text_off != OWXML_NONE) ? n->text_len : 0u;
    uint32_t total = old_len + run_len;
    owxml_index_t base;
    uint32_t i;

    if (total == 0u) {
        return true;
    }
    if (p->str_len + total + 1u > (uint32_t)OWXML_STRING_POOL) {
        p->error = true;
        return false;
    }

    base = p->str_len;
    for (i = 0u; i < old_len; ++i) {
        g_xml_strings[base + i] = g_xml_strings[n->text_off + i];
    }
    for (i = 0u; i < run_len; ++i) {
        g_xml_strings[base + old_len + i] = run[i];
    }
    g_xml_strings[base + total] = '\0';

    p->str_len = base + total + 1u;
    n->text_off = base;
    n->text_len = total;
    return true;
}

static bool xml_flush_text(ow_xml_parser_t *p, owxml_index_t node,
                           char *tmp, uint32_t *w)
{
    if (*w == 0u) {
        return true;
    }
    if (!xml_append_run(p, node, tmp, *w)) {
        return false;
    }
    *w = 0u;
    return true;
}

static bool xml_emit(ow_xml_parser_t *p, owxml_index_t node,
                     char *tmp, uint32_t *w, uint8_t b)
{
    if (*w >= (uint32_t)OWXML_TEXT_CHUNK) {
        if (!xml_flush_text(p, node, tmp, w)) {
            return false;
        }
    }
    tmp[*w] = (char)b;
    (*w)++;
    return true;
}

static uint32_t xml_encode_utf8(uint32_t cp, uint8_t out[4])
{
    if (cp < 0x80u) {
        out[0] = (uint8_t)cp;
        return 1u;
    }
    if (cp < 0x800u) {
        out[0] = (uint8_t)(0xC0u | (cp >> 6));
        out[1] = (uint8_t)(0x80u | (cp & 0x3Fu));
        return 2u;
    }
    if (cp < 0x10000u) {
        out[0] = (uint8_t)(0xE0u | (cp >> 12));
        out[1] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (uint8_t)(0x80u | (cp & 0x3Fu));
        return 3u;
    }
    out[0] = (uint8_t)(0xF0u | (cp >> 18));
    out[1] = (uint8_t)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (uint8_t)(0x80u | (cp & 0x3Fu));
    return 4u;
}

/* ── Lexical helpers ──────────────────────────────────────────── */

/*
 * Decode one entity reference (the leading '&' is already consumed) into
 * `out` (capacity `cap`, at most four bytes are produced).
 */
static bool xml_decode_entity(ow_xml_parser_t *p,
                              uint8_t *out, uint32_t cap, uint32_t *out_len)
{
    uint8_t buf[4];
    uint32_t n = 0u;
    uint32_t i;

    if (xml_match(p, "amp;")) {
        buf[0] = (uint8_t)'&';
        n = 1u;
    } else if (xml_match(p, "lt;")) {
        buf[0] = (uint8_t)'<';
        n = 1u;
    } else if (xml_match(p, "gt;")) {
        buf[0] = (uint8_t)'>';
        n = 1u;
    } else if (xml_match(p, "quot;")) {
        buf[0] = (uint8_t)'"';
        n = 1u;
    } else if (xml_match(p, "apos;")) {
        buf[0] = (uint8_t)'\'';
        n = 1u;
    } else if (xml_peek(p) == '#') {
        bool hex = false;
        bool any = false;
        uint32_t cp = 0u;

        p->pos++;
        if (xml_peek(p) == 'x' || xml_peek(p) == 'X') {
            hex = true;
            p->pos++;
        }
        while (p->pos < p->len) {
            int c = xml_peek(p);
            int d = -1;

            if (c >= '0' && c <= '9') {
                d = c - '0';
            } else if (hex && c >= 'a' && c <= 'f') {
                d = c - 'a' + 10;
            } else if (hex && c >= 'A' && c <= 'F') {
                d = c - 'A' + 10;
            } else {
                break;
            }
            cp = (cp * (hex ? 16u : 10u)) + (uint32_t)d;
            p->pos++;
            any = true;
        }
        if (!any || xml_peek(p) != ';') {
            return false;
        }
        p->pos++;
        n = xml_encode_utf8(cp, buf);
    } else {
        return false;
    }

    if (n > cap) {
        return false;
    }
    for (i = 0u; i < n; ++i) {
        out[i] = buf[i];
    }
    *out_len = n;
    return true;
}

/* Read an element / attribute name into the string pool. */
static owxml_index_t xml_parse_name(ow_xml_parser_t *p)
{
    owxml_index_t base;
    uint32_t w = 0u;

    if (p->str_len >= (uint32_t)OWXML_STRING_POOL) {
        p->error = true;
        return OWXML_NONE;
    }
    base = p->str_len;

    while (p->pos < p->len && xml_is_name_char(xml_peek(p))) {
        if (!xml_pool_put(base, &w, (uint8_t)p->text[p->pos])) {
            p->error = true;
            return OWXML_NONE;
        }
        p->pos++;
    }
    if (w == 0u) {
        p->error = true;
        return OWXML_NONE;
    }
    g_xml_strings[base + w] = '\0';
    p->str_len = base + w + 1u;
    return base;
}

/* Read a quoted attribute value (entities decoded) into the pool. */
static owxml_index_t xml_parse_attr_value(ow_xml_parser_t *p)
{
    int quote = xml_peek(p);
    owxml_index_t base;
    uint32_t w = 0u;

    if (quote != '"' && quote != '\'') {
        p->error = true;
        return OWXML_NONE;
    }
    p->pos++;

    if (p->str_len >= (uint32_t)OWXML_STRING_POOL) {
        p->error = true;
        return OWXML_NONE;
    }
    base = p->str_len;

    for (;;) {
        int c = xml_peek(p);

        if (c < 0) {
            p->error = true;
            return OWXML_NONE;
        }
        p->pos++;

        if (c == quote) {
            break;
        }
        if (c == '&') {
            uint8_t eb[4];
            uint32_t en = 0u;
            uint32_t i;

            if (!xml_decode_entity(p, eb, 4u, &en)) {
                p->error = true;
                return OWXML_NONE;
            }
            for (i = 0u; i < en; ++i) {
                if (!xml_pool_put(base, &w, eb[i])) {
                    p->error = true;
                    return OWXML_NONE;
                }
            }
            continue;
        }
        if (!xml_pool_put(base, &w, (uint8_t)c)) {
            p->error = true;
            return OWXML_NONE;
        }
    }

    g_xml_strings[base + w] = '\0';
    p->str_len = base + w + 1u;
    return base;
}

/* ── Document scanning ────────────────────────────────────────── */

static bool xml_skip_to(ow_xml_parser_t *p, const char *end, size_t end_len)
{
    while (p->pos < p->len) {
        if (p->pos + end_len <= p->len) {
            size_t i = 0u;
            bool hit = true;

            while (i < end_len) {
                if (p->text[p->pos + i] != end[i]) {
                    hit = false;
                    break;
                }
                i++;
            }
            if (hit) {
                p->pos += end_len;
                return true;
            }
        }
        p->pos++;
    }
    return false;
}

/* Collect character data (with entity expansion) into the current node. */
static bool xml_capture_text(ow_xml_parser_t *p, owxml_index_t node)
{
    char tmp[OWXML_TEXT_CHUNK];
    uint32_t w = 0u;

    while (p->pos < p->len) {
        int c = xml_peek(p);

        if (c == '<') {
            break;
        }
        p->pos++;

        if (c == '&') {
            uint8_t eb[4];
            uint32_t en = 0u;
            uint32_t i;

            if (!xml_decode_entity(p, eb, 4u, &en)) {
                p->error = true;
                return false;
            }
            for (i = 0u; i < en; ++i) {
                if (!xml_emit(p, node, tmp, &w, eb[i])) {
                    p->error = true;
                    return false;
                }
            }
        } else if (!xml_emit(p, node, tmp, &w, (uint8_t)c)) {
            p->error = true;
            return false;
        }
    }

    if (!xml_flush_text(p, node, tmp, &w)) {
        p->error = true;
        return false;
    }
    return true;
}

/* Collect a CDATA section verbatim into the current node. */
static bool xml_capture_cdata(ow_xml_parser_t *p, owxml_index_t node)
{
    char tmp[OWXML_TEXT_CHUNK];
    uint32_t w = 0u;
    bool closed = false;

    while (p->pos < p->len) {
        if (p->pos + 3u <= p->len && p->text[p->pos] == ']' &&
            p->text[p->pos + 1u] == ']' && p->text[p->pos + 2u] == '>') {
            p->pos += 3u;
            closed = true;
            break;
        }
        if (!xml_emit(p, node, tmp, &w, (uint8_t)p->text[p->pos])) {
            p->error = true;
            return false;
        }
        p->pos++;
    }

    if (!xml_flush_text(p, node, tmp, &w)) {
        p->error = true;
        return false;
    }
    if (!closed) {
        p->error = true;
        return false;
    }
    return true;
}

/* ── Element parsing ──────────────────────────────────────────── */

static bool xml_parse_start_tag(ow_xml_parser_t *p)
{
    owxml_index_t node;
    owxml_index_t name;
    bool self_close = false;

    p->pos++;   /* '<' */
    node = xml_alloc_node(p);
    if (node == OWXML_NONE) {
        return false;
    }
    name = xml_parse_name(p);
    if (name == OWXML_NONE) {
        return false;
    }
    g_xml_nodes[node].tag_off = name;

    for (;;) {
        xml_skip_ws(p);

        if (xml_peek(p) == '/') {
            p->pos++;
            if (xml_peek(p) != '>') {
                p->error = true;
                return false;
            }
            p->pos++;
            self_close = true;
            break;
        }
        if (xml_peek(p) == '>') {
            p->pos++;
            break;
        }
        if (xml_peek(p) < 0) {
            p->error = true;
            return false;
        }

        {
            owxml_index_t aname = xml_parse_name(p);

            if (aname == OWXML_NONE) {
                return false;
            }
            xml_skip_ws(p);
            if (xml_peek(p) != '=') {
                p->error = true;
                return false;
            }
            p->pos++;
            xml_skip_ws(p);

            {
                owxml_index_t avalue = xml_parse_attr_value(p);

                if (avalue == OWXML_NONE) {
                    return false;
                }
                xml_add_attr(p, node, aname, avalue);
                if (p->error) {
                    return false;
                }
            }
        }
    }

    /* Link into the tree. */
    if (p->depth > 0u) {
        owxml_index_t parent = p->stack[p->depth - 1u];

        g_xml_nodes[node].parent = parent;
        if (g_xml_nodes[parent].first_child == OWXML_NONE) {
            g_xml_nodes[parent].first_child = node;
        } else {
            owxml_index_t cur = g_xml_nodes[parent].first_child;
            while (g_xml_nodes[cur].next_sibling != OWXML_NONE) {
                cur = g_xml_nodes[cur].next_sibling;
            }
            g_xml_nodes[cur].next_sibling = node;
        }
    } else {
        if (p->root != OWXML_NONE) {
            p->error = true;   /* Multiple root elements */
            return false;
        }
        p->root = node;
    }

    if (!self_close) {
        if (p->depth >= (uint32_t)OWXML_MAX_DEPTH) {
            p->error = true;
            return false;
        }
        p->stack[p->depth++] = node;
    }
    return true;
}

static bool xml_parse_end_tag(ow_xml_parser_t *p)
{
    owxml_index_t name;
    owxml_index_t open;

    /* "</" has already been consumed by the caller. */
    name = xml_parse_name(p);
    if (name == OWXML_NONE) {
        return false;
    }
    xml_skip_ws(p);
    if (xml_peek(p) != '>') {
        p->error = true;
        return false;
    }
    p->pos++;

    if (p->depth == 0u) {
        p->error = true;
        return false;
    }
    open = p->stack[--p->depth];
    if (!xml_pool_streq(&g_xml_strings[g_xml_nodes[open].tag_off],
                        &g_xml_strings[name])) {
        p->error = true;   /* Mismatched end tag */
        return false;
    }
    return true;
}

/* ── Public API ───────────────────────────────────────────────── */

void owxml_init(void)
{
    uint32_t i;

    g_xml_root = OWXML_NONE;
    owxml64_ready = false;
    g_xml_strings[0] = '\0';
    for (i = 0u; i < (uint32_t)OWXML_MAX_NODES; ++i) {
        g_xml_nodes[i].tag_off = OWXML_NONE;
        g_xml_nodes[i].text_off = OWXML_NONE;
        g_xml_nodes[i].first_attr = OWXML_NONE;
        g_xml_nodes[i].first_child = OWXML_NONE;
        g_xml_nodes[i].next_sibling = OWXML_NONE;
        g_xml_nodes[i].parent = OWXML_NONE;
        g_xml_nodes[i].text_len = 0u;
    }
    for (i = 0u; i < (uint32_t)OWXML_MAX_ATTRS; ++i) {
        g_xml_attrs[i].name_off = OWXML_NONE;
        g_xml_attrs[i].value_off = OWXML_NONE;
        g_xml_attrs[i].next = OWXML_NONE;
    }
}

bool owxml_parse(const char *text, size_t length)
{
    ow_xml_parser_t p;

    owxml_init();

    if (!text) {
        return false;
    }

    p.text       = text;
    p.len        = length;
    p.pos        = 0u;
    p.node_count = 0u;
    p.attr_count = 0u;
    p.str_len    = 1u;      /* Offset 0 is the empty string */
    p.root       = OWXML_NONE;
    p.error      = false;
    p.depth      = 0u;

    while (p.pos < p.len && !p.error) {
        if (xml_peek(&p) == '<') {
            if (xml_match(&p, "<!--")) {
                if (!xml_skip_to(&p, "-->", 3u)) {
                    p.error = true;
                }
            } else if (xml_match(&p, "<![CDATA[")) {
                if (p.depth == 0u) {
                    p.error = true;   /* CDATA outside the root */
                } else {
                    (void)xml_capture_cdata(&p, p.stack[p.depth - 1u]);
                }
            } else if (xml_match(&p, "<?")) {
                if (!xml_skip_to(&p, "?>", 2u)) {
                    p.error = true;
                }
            } else if (xml_match(&p, "<!")) {
                if (!xml_skip_to(&p, ">", 1u)) {
                    p.error = true;
                }
            } else if (xml_match(&p, "</")) {
                (void)xml_parse_end_tag(&p);
            } else {
                (void)xml_parse_start_tag(&p);
            }
        } else {
            if (p.depth == 0u) {
                if (xml_isspace(xml_peek(&p))) {
                    p.pos++;
                } else {
                    p.error = true;   /* Character data outside the root */
                }
            } else {
                (void)xml_capture_text(&p, p.stack[p.depth - 1u]);
            }
        }
    }

    if (!p.error && p.depth != 0u) {
        p.error = true;   /* Unclosed element(s) */
    }
    if (p.error || p.root == OWXML_NONE) {
        owxml_free();
        return false;
    }

    g_xml_root = p.root;
    owxml64_ready = true;
    return true;
}

const owxml_node_t *owxml_get_root(void)
{
    if (!owxml64_ready || g_xml_root == OWXML_NONE) {
        return NULL;
    }
    return &g_xml_nodes[g_xml_root];
}

const char *owxml_get_name(const owxml_node_t *node)
{
    if (!node || node->tag_off == OWXML_NONE ||
        node->tag_off >= (owxml_index_t)OWXML_STRING_POOL) {
        return NULL;
    }
    return &g_xml_strings[node->tag_off];
}

const char *owxml_get_text(const owxml_node_t *node)
{
    if (!node || node->text_off == OWXML_NONE ||
        node->text_off >= (owxml_index_t)OWXML_STRING_POOL) {
        return NULL;
    }
    return &g_xml_strings[node->text_off];
}

static const owxml_node_t *xml_find_recursive(const owxml_node_t *scope,
                                              const char *tag)
{
    owxml_index_t cur = scope->first_child;

    while (cur != OWXML_NONE) {
        const owxml_node_t *node = &g_xml_nodes[cur];
        const owxml_node_t *hit;

        if (node->tag_off != OWXML_NONE &&
            xml_pool_streq(&g_xml_strings[node->tag_off], tag)) {
            return node;
        }
        hit = xml_find_recursive(node, tag);
        if (hit) {
            return hit;
        }
        cur = node->next_sibling;
    }
    return NULL;
}

const owxml_node_t *owxml_find_element(const owxml_node_t *scope,
                                       const char *tag)
{
    const owxml_node_t *start;

    if (!owxml64_ready || !tag) {
        return NULL;
    }

    start = scope ? scope : &g_xml_nodes[g_xml_root];
    if (start->tag_off != OWXML_NONE &&
        xml_pool_streq(&g_xml_strings[start->tag_off], tag)) {
        return start;
    }
    return xml_find_recursive(start, tag);
}

const char *owxml_get_attr(const owxml_node_t *node, const char *name)
{
    owxml_index_t a;

    if (!node || !name) {
        return NULL;
    }

    a = node->first_attr;
    while (a != OWXML_NONE && a < (owxml_index_t)OWXML_MAX_ATTRS) {
        const owxml_attr_t *attr = &g_xml_attrs[a];

        if (attr->name_off != OWXML_NONE &&
            xml_pool_streq(&g_xml_strings[attr->name_off], name)) {
            return &g_xml_strings[attr->value_off];
        }
        a = attr->next;
    }
    return NULL;
}

const owxml_node_t *owxml_first_child(const owxml_node_t *node)
{
    if (!node || node->first_child == OWXML_NONE) {
        return NULL;
    }
    return &g_xml_nodes[node->first_child];
}

const owxml_node_t *owxml_next_sibling(const owxml_node_t *node)
{
    if (!node || node->next_sibling == OWXML_NONE) {
        return NULL;
    }
    return &g_xml_nodes[node->next_sibling];
}

void owxml_free(void)
{
    g_xml_root = OWXML_NONE;
    owxml64_ready = false;
}
