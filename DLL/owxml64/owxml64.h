/*
 * owxml64.h - OpenWindows Minimal XML Parser (.owd)
 *
 * A dependency-free, non-validating XML reader. Elements, attributes,
 * nested children, text content, comments, processing instructions, CDATA
 * sections and the five predefined entities are handled. Decoded text is
 * stored in a fixed string pool; the tree lives in fixed static pools.
 *
 * A document may declare any encoding, but bytes are treated as their raw
 * 8-bit values (ASCII-compatible sources are decoded verbatim).
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWXML64_H
#define OWXML64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Fixed capacities (documented static storage). */
#define OWXML_MAX_NODES    128u     /* Element nodes                     */
#define OWXML_MAX_ATTRS    256u     /* Attribute records                 */
#define OWXML_STRING_POOL  2048u    /* Decoded names / values / text     */
#define OWXML_MAX_DEPTH    32u      /* Maximum element nesting           */
#define OWXML_TEXT_CHUNK   256u     /* Text decode scratch width         */

/* Pool indices; OWXML_NONE marks "absent". */
typedef uint32_t owxml_index_t;
#define OWXML_NONE ((owxml_index_t)0xFFFFFFFFu)

/* One element node. */
typedef struct {
    owxml_index_t tag_off;      /* Element name pool offset            */
    owxml_index_t text_off;     /* Decoded text offset, or OWXML_NONE  */
    owxml_index_t first_attr;   /* First attribute index, or OWXML_NONE */
    owxml_index_t first_child;  /* First child element, or OWXML_NONE  */
    owxml_index_t next_sibling; /* Next sibling element, or OWXML_NONE */
    owxml_index_t parent;       /* Parent element, or OWXML_NONE       */
    uint32_t      text_len;     /* Decoded text length in bytes        */
} owxml_node_t;

/* One attribute record. */
typedef struct {
    owxml_index_t name_off;     /* Attribute name pool offset          */
    owxml_index_t value_off;    /* Decoded value pool offset           */
    owxml_index_t next;         /* Next attribute, or OWXML_NONE       */
} owxml_attr_t;

/* Reset the parser and clear the node / attribute / string pools. */
void owxml_init(void);

/* Parse `length` bytes of XML text. Returns true on a well-formed parse. */
bool owxml_parse(const char *text, size_t length);

/* Root element of the last successful parse, or NULL if none. */
const owxml_node_t *owxml_get_root(void);

/* Element name of `node`, or NULL for a NULL node. */
const char *owxml_get_name(const owxml_node_t *node);

/* Decoded text content of `node`, or NULL when it has no text. */
const char *owxml_get_text(const owxml_node_t *node);

/*
 * Depth-first search for the first element tagged `tag` at or below
 * `scope`. Pass NULL for `scope` to start at the document root. Matching
 * is case-sensitive and ignores namespaces.
 */
const owxml_node_t *owxml_find_element(const owxml_node_t *scope,
                                       const char *tag);

/* Value of attribute `name` on `node`, or NULL if absent. */
const char *owxml_get_attr(const owxml_node_t *node, const char *name);

/* First child element of `node`, or NULL. */
const owxml_node_t *owxml_first_child(const owxml_node_t *node);

/* Next sibling element of `node`, or NULL. */
const owxml_node_t *owxml_next_sibling(const owxml_node_t *node);

/* Release the parsed document (clears the root; pools become reusable). */
void owxml_free(void);

#endif /* OWXML64_H */
