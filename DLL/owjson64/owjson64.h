/*
 * owjson64.h - OpenWindows Minimal JSON Parser (.owd)
 *
 * A dependency-free RFC 8259 subset parser. JSON text is decoded into a
 * fixed, file-scope node pool; strings are decoded (escapes resolved,
 * UTF-8 encoded) into a fixed string pool. No heap allocation.
 *
 * Supported values: object, array, string, number, boolean, null.
 * Escape sequences: \" \\ \/ \b \f \n \r \t and \uXXXX (with surrogate
 * pairs). Embedded NUL characters inside strings terminate the string.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWJSON64_H
#define OWJSON64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Fixed capacities (documented static storage). */
#define OWJSON_MAX_NODES    128u    /* Parser node pool depth/width     */
#define OWJSON_STRING_POOL  1024u   /* Decoded string bytes (incl. NUL) */
#define OWJSON_MAX_DEPTH    32u     /* Maximum container nesting        */

/* Node indices; OWJSON_NONE marks "no such node". */
typedef uint32_t owjson_index_t;
#define OWJSON_NONE ((owjson_index_t)0xFFFFFFFFu)

/* Value kinds held by a node. */
typedef enum {
    OWJSON_TYPE_NULL = 0,
    OWJSON_TYPE_BOOL,
    OWJSON_TYPE_NUMBER,
    OWJSON_TYPE_STRING,
    OWJSON_TYPE_ARRAY,
    OWJSON_TYPE_OBJECT
} owjson_type_t;

/* One decoded JSON value. Object members reuse the value node and carry
 * their member key in `key_off`. Siblings are chained through `next`. */
typedef struct {
    owjson_type_t  type;
    owjson_index_t next;        /* Next sibling, or OWJSON_NONE        */
    owjson_index_t first_child; /* First array element / object member */
    owjson_index_t key_off;     /* Object member key string offset     */
    double         number;      /* OWJSON_TYPE_NUMBER value            */
    bool           boolean;     /* OWJSON_TYPE_BOOL value              */
    owjson_index_t string_off;  /* OWJSON_TYPE_STRING pool offset      */
} owjson_node_t;

/* Reset the parser and clear the node/string pools. */
void owjson_init(void);

/* Parse `length` bytes of JSON text. Returns true on full, valid parse. */
bool owjson_parse(const char *text, size_t length);

/* Root value of the last successful parse, or NULL if none. */
const owjson_node_t *owjson_get_root(void);

/*
 * Resolve a dot/bracket path (e.g. "server.ports[2].name") starting at
 * `scope`. Pass NULL for `scope` to start at the root; pass NULL for `path`
 * to return `scope` (or the root). Returns NULL when the path is absent.
 */
const owjson_node_t *owjson_find(const owjson_node_t *scope, const char *path);

/* String view of a string node, or NULL for a non-string / empty node. */
const char *owjson_get_string(const owjson_node_t *node);

/* Extract a number value; returns false for a non-number node. */
bool owjson_get_number(const owjson_node_t *node, double *out);

/* Extract a boolean value; returns false for a non-boolean node. */
bool owjson_get_bool(const owjson_node_t *node, bool *out);

/* Indexed array element access; returns NULL when out of range / not array. */
const owjson_node_t *owjson_get_array(const owjson_node_t *node, uint32_t index);

/* Release the parsed document (clears root and marks the pool reusable). */
void owjson_free(void);

#endif /* OWJSON64_H */
