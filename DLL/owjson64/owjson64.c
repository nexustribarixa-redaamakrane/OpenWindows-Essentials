/*
 * owjson64.c - OpenWindows Minimal JSON Parser (.owd)
 *
 * Recursive-descent RFC 8259 subset parser over fixed static pools.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owjson64.h"

/* ── Internal state ───────────────────────────────────────────── */

static owjson_node_t  g_json_nodes[OWJSON_MAX_NODES];
static char           g_json_strings[OWJSON_STRING_POOL];
static owjson_index_t g_json_root = OWJSON_NONE;
static bool           owjson64_ready = false;

typedef struct {
    const char    *text;
    size_t         len;
    size_t         pos;
    owjson_index_t node_count;
    uint32_t       str_len;
    bool           error;
} ow_json_parser_t;

/* ── Character classification ─────────────────────────────────── */

static bool json_isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

static bool json_isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static int json_peek(const ow_json_parser_t *p)
{
    if (p->pos >= p->len) {
        return -1;
    }
    return (int)(unsigned char)p->text[p->pos];
}

static void json_skip_ws(ow_json_parser_t *p)
{
    while (p->pos < p->len && json_isspace(json_peek(p))) {
        p->pos++;
    }
}

/* ── Pools ────────────────────────────────────────────────────── */

static owjson_index_t json_alloc(ow_json_parser_t *p)
{
    owjson_node_t *n;

    if (p->node_count >= (owjson_index_t)OWJSON_MAX_NODES) {
        p->error = true;
        return OWJSON_NONE;
    }

    n = &g_json_nodes[p->node_count];
    n->type        = OWJSON_TYPE_NULL;
    n->next        = OWJSON_NONE;
    n->first_child = OWJSON_NONE;
    n->key_off     = OWJSON_NONE;
    n->number      = 0.0;
    n->boolean     = false;
    n->string_off  = OWJSON_NONE;

    return p->node_count++;
}

/* Append one byte to the string pool, leaving room for a NUL terminator. */
static bool json_pool_put(owjson_index_t base, uint32_t *w, uint8_t ch)
{
    if ((uint32_t)base + *w + 2u > (uint32_t)OWJSON_STRING_POOL) {
        return false;
    }
    g_json_strings[base + *w] = (char)ch;
    (*w)++;
    return true;
}

/* Encode a Unicode code point as UTF-8 into the string pool. */
static bool json_pool_utf8(owjson_index_t base, uint32_t *w, uint32_t cp)
{
    uint8_t b[4];
    uint32_t n;
    uint32_t i;

    if (cp < 0x80u) {
        b[0] = (uint8_t)cp;
        n = 1u;
    } else if (cp < 0x800u) {
        b[0] = (uint8_t)(0xC0u | (cp >> 6));
        b[1] = (uint8_t)(0x80u | (cp & 0x3Fu));
        n = 2u;
    } else if (cp < 0x10000u) {
        b[0] = (uint8_t)(0xE0u | (cp >> 12));
        b[1] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
        b[2] = (uint8_t)(0x80u | (cp & 0x3Fu));
        n = 3u;
    } else {
        b[0] = (uint8_t)(0xF0u | (cp >> 18));
        b[1] = (uint8_t)(0x80u | ((cp >> 12) & 0x3Fu));
        b[2] = (uint8_t)(0x80u | ((cp >> 6) & 0x3Fu));
        b[3] = (uint8_t)(0x80u | (cp & 0x3Fu));
        n = 4u;
    }

    for (i = 0u; i < n; ++i) {
        if (!json_pool_put(base, w, b[i])) {
            return false;
        }
    }
    return true;
}

/* Read exactly four hexadecimal digits. */
static bool json_read_hex4(ow_json_parser_t *p, uint32_t *out)
{
    uint32_t v = 0u;
    uint32_t i;

    for (i = 0u; i < 4u; ++i) {
        int c = json_peek(p);
        uint32_t d;

        if (c < 0) {
            return false;
        }
        p->pos++;

        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (uint32_t)(c - 'a') + 10u;
        } else if (c >= 'A' && c <= 'F') {
            d = (uint32_t)(c - 'A') + 10u;
        } else {
            return false;
        }
        v = (v << 4) | d;
    }

    *out = v;
    return true;
}

/* ── String parsing ───────────────────────────────────────────── */

/* Decode a quoted JSON string into the pool; returns its pool offset. */
static owjson_index_t json_parse_string(ow_json_parser_t *p)
{
    owjson_index_t base;
    uint32_t w = 0u;

    if (json_peek(p) != '"') {
        p->error = true;
        return OWJSON_NONE;
    }
    p->pos++;

    if (p->str_len >= (uint32_t)OWJSON_STRING_POOL) {
        p->error = true;
        return OWJSON_NONE;
    }
    base = p->str_len;

    for (;;) {
        int c = json_peek(p);

        if (c < 0) {
            p->error = true;
            return OWJSON_NONE;
        }
        p->pos++;

        if (c == '"') {
            break;
        }

        if (c == '\\') {
            int e = json_peek(p);

            if (e < 0) {
                p->error = true;
                return OWJSON_NONE;
            }
            p->pos++;

            switch (e) {
            case '"':  c = '"';  break;
            case '\\': c = '\\'; break;
            case '/':  c = '/';  break;
            case 'b':  c = '\b'; break;
            case 'f':  c = '\f'; break;
            case 'n':  c = '\n'; break;
            case 'r':  c = '\r'; break;
            case 't':  c = '\t'; break;
            case 'u': {
                uint32_t cp;

                if (!json_read_hex4(p, &cp)) {
                    p->error = true;
                    return OWJSON_NONE;
                }
                if (cp >= 0xD800u && cp <= 0xDBFFu) {
                    /* High surrogate: try to pair with a low surrogate. */
                    if (json_peek(p) == '\\') {
                        p->pos++;
                        if (json_peek(p) == 'u') {
                            uint32_t lo;

                            p->pos++;
                            if (!json_read_hex4(p, &lo)) {
                                p->error = true;
                                return OWJSON_NONE;
                            }
                            if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                                cp = 0x10000u + ((cp - 0xD800u) << 10) +
                                     (lo - 0xDC00u);
                            } else {
                                cp = 0xFFFDu;   /* U+FFFD replacement */
                            }
                        } else {
                            p->error = true;
                            return OWJSON_NONE;
                        }
                    } else {
                        cp = 0xFFFDu;
                    }
                } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {
                    cp = 0xFFFDu;   /* Lone low surrogate */
                }

                if (!json_pool_utf8(base, &w, cp)) {
                    p->error = true;
                    return OWJSON_NONE;
                }
                continue;
            }
            default:
                p->error = true;
                return OWJSON_NONE;
            }
        } else if (c < 0x20) {
            p->error = true;   /* Raw control character in string */
            return OWJSON_NONE;
        }

        if (!json_pool_put(base, &w, (uint8_t)c)) {
            p->error = true;
            return OWJSON_NONE;
        }
    }

    g_json_strings[base + w] = '\0';
    p->str_len = base + w + 1u;
    return base;
}

/* ── Number parsing ───────────────────────────────────────────── */

static double json_pow10(int exp, bool *ok)
{
    double r = 1.0;
    int i;

    if (exp > 308 || exp < -308) {
        *ok = false;
        return 0.0;
    }
    if (exp >= 0) {
        for (i = 0; i < exp; ++i) {
            r *= 10.0;
        }
    } else {
        for (i = 0; i < -exp; ++i) {
            r /= 10.0;
        }
    }
    return r;
}

static bool json_parse_number(ow_json_parser_t *p, double *out)
{
    bool negative = false;
    bool ok = true;
    double mantissa = 0.0;
    int frac_digits = 0;
    int exponent = 0;
    bool have_digit = false;

    if (json_peek(p) == '-') {
        negative = true;
        p->pos++;
    }

    while (json_isdigit(json_peek(p))) {
        mantissa = (mantissa * 10.0) + (double)(json_peek(p) - '0');
        p->pos++;
        have_digit = true;
    }
    if (!have_digit) {
        return false;
    }

    if (json_peek(p) == '.') {
        p->pos++;
        if (!json_isdigit(json_peek(p))) {
            return false;
        }
        while (json_isdigit(json_peek(p))) {
            mantissa = (mantissa * 10.0) + (double)(json_peek(p) - '0');
            p->pos++;
            frac_digits++;
        }
    }

    if (json_peek(p) == 'e' || json_peek(p) == 'E') {
        bool exp_neg = false;

        p->pos++;
        if (json_peek(p) == '+' || json_peek(p) == '-') {
            exp_neg = (json_peek(p) == '-');
            p->pos++;
        }
        if (!json_isdigit(json_peek(p))) {
            return false;
        }
        while (json_isdigit(json_peek(p))) {
            if (exponent < 100000) {
                exponent = (exponent * 10) + (json_peek(p) - '0');
            }
            p->pos++;
        }
        if (exp_neg) {
            exponent = -exponent;
        }
    }

    {
        double scale = json_pow10(exponent - frac_digits, &ok);
        if (!ok) {
            return false;
        }
        *out = negative ? -(mantissa * scale) : (mantissa * scale);
    }
    return true;
}

/* ── Value parsing ────────────────────────────────────────────── */

static owjson_index_t json_parse_value(ow_json_parser_t *p, uint32_t depth);

/* Match one of true/false/null literally. */
static bool json_match_word(ow_json_parser_t *p, const char *word)
{
    size_t i = 0u;

    while (word[i] != '\0') {
        if (p->pos + i >= p->len || p->text[p->pos + i] != word[i]) {
            return false;
        }
        i++;
    }
    p->pos += i;
    return true;
}

static owjson_index_t json_parse_array(ow_json_parser_t *p, uint32_t depth)
{
    owjson_index_t head = OWJSON_NONE;
    owjson_index_t tail = OWJSON_NONE;
    owjson_index_t node;

    p->pos++;   /* consume '[' */
    json_skip_ws(p);

    if (json_peek(p) == ']') {
        p->pos++;
        node = json_alloc(p);
        if (node == OWJSON_NONE) {
            return OWJSON_NONE;
        }
        g_json_nodes[node].type = OWJSON_TYPE_ARRAY;
        return node;
    }

    for (;;) {
        owjson_index_t val = json_parse_value(p, depth + 1u);

        if (p->error || val == OWJSON_NONE) {
            p->error = true;
            return OWJSON_NONE;
        }
        if (head == OWJSON_NONE) {
            head = val;
        } else {
            g_json_nodes[tail].next = val;
        }
        tail = val;

        json_skip_ws(p);
        if (json_peek(p) == ',') {
            p->pos++;
            json_skip_ws(p);
            continue;
        }
        if (json_peek(p) == ']') {
            p->pos++;
            break;
        }
        p->error = true;
        return OWJSON_NONE;
    }

    node = json_alloc(p);
    if (node == OWJSON_NONE) {
        return OWJSON_NONE;
    }
    g_json_nodes[node].type = OWJSON_TYPE_ARRAY;
    g_json_nodes[node].first_child = head;
    return node;
}

static owjson_index_t json_parse_object(ow_json_parser_t *p, uint32_t depth)
{
    owjson_index_t head = OWJSON_NONE;
    owjson_index_t tail = OWJSON_NONE;
    owjson_index_t node;

    p->pos++;   /* consume '{' */
    json_skip_ws(p);

    if (json_peek(p) == '}') {
        p->pos++;
        node = json_alloc(p);
        if (node == OWJSON_NONE) {
            return OWJSON_NONE;
        }
        g_json_nodes[node].type = OWJSON_TYPE_OBJECT;
        return node;
    }

    for (;;) {
        owjson_index_t key;
        owjson_index_t val;

        json_skip_ws(p);
        key = json_parse_string(p);
        if (p->error || key == OWJSON_NONE) {
            p->error = true;
            return OWJSON_NONE;
        }

        json_skip_ws(p);
        if (json_peek(p) != ':') {
            p->error = true;
            return OWJSON_NONE;
        }
        p->pos++;

        val = json_parse_value(p, depth + 1u);
        if (p->error || val == OWJSON_NONE) {
            p->error = true;
            return OWJSON_NONE;
        }
        g_json_nodes[val].key_off = key;

        if (head == OWJSON_NONE) {
            head = val;
        } else {
            g_json_nodes[tail].next = val;
        }
        tail = val;

        json_skip_ws(p);
        if (json_peek(p) == ',') {
            p->pos++;
            continue;
        }
        if (json_peek(p) == '}') {
            p->pos++;
            break;
        }
        p->error = true;
        return OWJSON_NONE;
    }

    node = json_alloc(p);
    if (node == OWJSON_NONE) {
        return OWJSON_NONE;
    }
    g_json_nodes[node].type = OWJSON_TYPE_OBJECT;
    g_json_nodes[node].first_child = head;
    return node;
}

static owjson_index_t json_parse_value(ow_json_parser_t *p, uint32_t depth)
{
    int c;
    owjson_index_t node;

    if (p->error) {
        return OWJSON_NONE;
    }
    if (depth > (uint32_t)OWJSON_MAX_DEPTH) {
        p->error = true;
        return OWJSON_NONE;
    }

    json_skip_ws(p);
    c = json_peek(p);
    if (c < 0) {
        p->error = true;
        return OWJSON_NONE;
    }

    switch (c) {
    case '{':
        return json_parse_object(p, depth);
    case '[':
        return json_parse_array(p, depth);
    case '"': {
        owjson_index_t off = json_parse_string(p);
        if (p->error || off == OWJSON_NONE) {
            p->error = true;
            return OWJSON_NONE;
        }
        node = json_alloc(p);
        if (node == OWJSON_NONE) {
            return OWJSON_NONE;
        }
        g_json_nodes[node].type = OWJSON_TYPE_STRING;
        g_json_nodes[node].string_off = off;
        return node;
    }
    case 't':
        if (!json_match_word(p, "true")) {
            p->error = true;
            return OWJSON_NONE;
        }
        node = json_alloc(p);
        if (node == OWJSON_NONE) {
            return OWJSON_NONE;
        }
        g_json_nodes[node].type = OWJSON_TYPE_BOOL;
        g_json_nodes[node].boolean = true;
        return node;
    case 'f':
        if (!json_match_word(p, "false")) {
            p->error = true;
            return OWJSON_NONE;
        }
        node = json_alloc(p);
        if (node == OWJSON_NONE) {
            return OWJSON_NONE;
        }
        g_json_nodes[node].type = OWJSON_TYPE_BOOL;
        g_json_nodes[node].boolean = false;
        return node;
    case 'n':
        if (!json_match_word(p, "null")) {
            p->error = true;
            return OWJSON_NONE;
        }
        node = json_alloc(p);
        if (node == OWJSON_NONE) {
            return OWJSON_NONE;
        }
        g_json_nodes[node].type = OWJSON_TYPE_NULL;
        return node;
    default:
        break;
    }

    if (c == '-' || json_isdigit(c)) {
        double number = 0.0;

        if (!json_parse_number(p, &number)) {
            p->error = true;
            return OWJSON_NONE;
        }
        node = json_alloc(p);
        if (node == OWJSON_NONE) {
            return OWJSON_NONE;
        }
        g_json_nodes[node].type = OWJSON_TYPE_NUMBER;
        g_json_nodes[node].number = number;
        return node;
    }

    p->error = true;
    return OWJSON_NONE;
}

/* ── Public API ───────────────────────────────────────────────── */

void owjson_init(void)
{
    uint32_t i;

    g_json_root = OWJSON_NONE;
    owjson64_ready = false;
    g_json_strings[0] = '\0';
    for (i = 0u; i < (uint32_t)OWJSON_MAX_NODES; ++i) {
        g_json_nodes[i].type = OWJSON_TYPE_NULL;
        g_json_nodes[i].next = OWJSON_NONE;
        g_json_nodes[i].first_child = OWJSON_NONE;
        g_json_nodes[i].key_off = OWJSON_NONE;
        g_json_nodes[i].string_off = OWJSON_NONE;
    }
}

bool owjson_parse(const char *text, size_t length)
{
    ow_json_parser_t p;
    owjson_index_t root;

    owjson_init();

    if (!text && length != 0u) {
        return false;
    }
    if (!text) {
        return false;
    }

    p.text       = text;
    p.len        = length;
    p.pos        = 0u;
    p.node_count = 0u;
    p.str_len    = 1u;      /* Offset 0 is the empty string */
    p.error      = false;

    root = json_parse_value(&p, 0u);
    if (p.error || root == OWJSON_NONE) {
        owjson_free();
        return false;
    }

    json_skip_ws(&p);
    if (p.pos != p.len) {
        owjson_free();
        return false;   /* Trailing garbage after the root value */
    }

    g_json_root = root;
    owjson64_ready = true;
    return true;
}

const owjson_node_t *owjson_get_root(void)
{
    if (!owjson64_ready || g_json_root == OWJSON_NONE) {
        return NULL;
    }
    return &g_json_nodes[g_json_root];
}

const char *owjson_get_string(const owjson_node_t *node)
{
    if (!node || node->type != OWJSON_TYPE_STRING) {
        return NULL;
    }
    if (node->string_off >= (owjson_index_t)OWJSON_STRING_POOL) {
        return NULL;
    }
    return &g_json_strings[node->string_off];
}

bool owjson_get_number(const owjson_node_t *node, double *out)
{
    if (!node || node->type != OWJSON_TYPE_NUMBER || !out) {
        return false;
    }
    *out = node->number;
    return true;
}

bool owjson_get_bool(const owjson_node_t *node, bool *out)
{
    if (!node || node->type != OWJSON_TYPE_BOOL || !out) {
        return false;
    }
    *out = node->boolean;
    return true;
}

const owjson_node_t *owjson_get_array(const owjson_node_t *node, uint32_t index)
{
    owjson_index_t cur;

    if (!node || node->type != OWJSON_TYPE_ARRAY) {
        return NULL;
    }

    cur = node->first_child;
    while (cur != OWJSON_NONE) {
        if (index == 0u) {
            return &g_json_nodes[cur];
        }
        index--;
        cur = g_json_nodes[cur].next;
    }
    return NULL;
}

/* ── Path lookup ──────────────────────────────────────────────── */

static bool json_key_equals(const char *stored, const char *key, size_t klen)
{
    size_t i;

    for (i = 0u; i < klen; ++i) {
        if (stored[i] == '\0' || stored[i] != key[i]) {
            return false;
        }
    }
    return (stored[klen] == '\0');
}

static const owjson_node_t *json_object_lookup(const owjson_node_t *obj,
                                               const char *key, size_t klen)
{
    owjson_index_t cur;

    if (!obj || obj->type != OWJSON_TYPE_OBJECT) {
        return NULL;
    }

    cur = obj->first_child;
    while (cur != OWJSON_NONE) {
        const owjson_node_t *m = &g_json_nodes[cur];

        if (m->key_off != OWJSON_NONE &&
            m->key_off < (owjson_index_t)OWJSON_STRING_POOL &&
            json_key_equals(&g_json_strings[m->key_off], key, klen)) {
            return m;
        }
        cur = m->next;
    }
    return NULL;
}

const owjson_node_t *owjson_find(const owjson_node_t *scope, const char *path)
{
    const owjson_node_t *node;
    const char *s;

    if (!owjson64_ready || g_json_root == OWJSON_NONE) {
        return NULL;
    }

    node = scope ? scope : &g_json_nodes[g_json_root];
    if (!path) {
        return node;
    }

    s = path;
    while (*s != '\0') {
        if (*s == '.') {
            s++;
            continue;
        }

        if (*s == '[') {
            uint32_t index = 0u;
            bool any = false;

            s++;
            while (json_isdigit((int)(unsigned char)*s)) {
                index = (index * 10u) + (uint32_t)(*s - '0');
                s++;
                any = true;
            }
            if (!any || *s != ']') {
                return NULL;
            }
            s++;
            node = owjson_get_array(node, index);
            if (!node) {
                return NULL;
            }
        } else {
            const char *start = s;

            while (*s != '\0' && *s != '.' && *s != '[') {
                s++;
            }
            if (s == start) {
                return NULL;
            }
            node = json_object_lookup(node, start, (size_t)(s - start));
            if (!node) {
                return NULL;
            }
        }
    }

    return node;
}

void owjson_free(void)
{
    g_json_root = OWJSON_NONE;
    owjson64_ready = false;
}
