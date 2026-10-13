/*
 * banhammer_cli_core.h - pure logic for the OpenWindows Banhammer panic-text CLI.
 *
 * Split out of banhammer_cli.c so the command/category/seed rules and the quote
 * selection can be driven by a host test with no kernel, no trap and no console:
 * nothing in this header performs I/O.  banhammer_cli.c supplies the argument
 * handling and the UART output; the host harness links these same functions
 * against the same banhammer_texts.h table the program is built with.
 *
 * The quote table and the pseudo-random selector live in
 * Extensions/banhammer_texts.h, the same header the panic path uses, so the CLI
 * can only ever print a quote the kernel itself would print for that seed.
 *
 * C99 freestanding.  Zero dynamic heap allocation.
 */
#ifndef BANHAMMER_CLI_CORE_H
#define BANHAMMER_CLI_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../Extensions/banhammer_texts.h"

/* Sub-commands the CLI recognises.  NONE means "no verb was supplied" and is
 * kept distinct from UNKNOWN so a bare `banhammer_cli` can print usage while
 * `banhammer_cli frobnicate` reports the offending verb. */
typedef enum {
    BH_CLI_CMD_NONE = 0,
    BH_CLI_CMD_HELP,
    BH_CLI_CMD_QUOTE,
    BH_CLI_CMD_LIST,
    BH_CLI_CMD_COUNT,
    BH_CLI_CMD_CATEGORIES,
    BH_CLI_CMD_UNKNOWN
} bh_cli_cmd_t;

/* Category names the CLI accepts.  UNSUPPORTED is a name the text header's enum
 * declares (BH_TEXT_RECOVERY) but for which no distinct table exists: the
 * selector's else branch folds recovery into the informational set.  Naming it
 * separately lets the CLI say that plainly instead of answering with the wrong
 * table and pretending the category is real. */
typedef enum {
    BH_CLI_CAT_SARCASTIC = 0,
    BH_CLI_CAT_INFORMATIONAL,
    BH_CLI_CAT_UNSUPPORTED,
    BH_CLI_CAT_UNKNOWN
} bh_cli_cat_t;

static inline size_t bh_cli_strlen(const char *text) {
    size_t n = 0u;
    if (text == NULL) {
        return 0u;
    }
    while (text[n] != '\0') {
        ++n;
    }
    return n;
}

static inline bool bh_cli_str_eq(const char *a, const char *b) {
    size_t i = 0u;
    if (a == NULL || b == NULL) {
        return false;
    }
    for (;;) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0') return true;
        ++i;
    }
}

static inline bh_cli_cmd_t bh_cli_classify(const char *verb) {
    if (verb == NULL) return BH_CLI_CMD_NONE;
    if (bh_cli_str_eq(verb, "help"))       return BH_CLI_CMD_HELP;
    if (bh_cli_str_eq(verb, "quote"))      return BH_CLI_CMD_QUOTE;
    if (bh_cli_str_eq(verb, "list"))       return BH_CLI_CMD_LIST;
    if (bh_cli_str_eq(verb, "count"))      return BH_CLI_CMD_COUNT;
    if (bh_cli_str_eq(verb, "categories")) return BH_CLI_CMD_CATEGORIES;
    return BH_CLI_CMD_UNKNOWN;
}

static inline bh_cli_cat_t bh_cli_category(const char *name) {
    if (name == NULL) return BH_CLI_CAT_UNKNOWN;
    if (bh_cli_str_eq(name, "sarcastic"))     return BH_CLI_CAT_SARCASTIC;
    if (bh_cli_str_eq(name, "informational")) return BH_CLI_CAT_INFORMATIONAL;
    if (bh_cli_str_eq(name, "recovery"))      return BH_CLI_CAT_UNSUPPORTED;
    return BH_CLI_CAT_UNKNOWN;
}

/* Canonical spelling of a category, or NULL when the value is not a backed
 * category.  Used for the categories listing and for echoing a resolved category
 * back to the user. */
static inline const char *bh_cli_category_name(bh_cli_cat_t cat) {
    switch (cat) {
    case BH_CLI_CAT_SARCASTIC:     return "sarcastic";
    case BH_CLI_CAT_INFORMATIONAL: return "informational";
    default:                       return (const char *)0;
    }
}

/* Number of quotes in a backed category; 0 for anything else. */
static inline uint32_t bh_cli_count(bh_cli_cat_t cat) {
    switch (cat) {
    case BH_CLI_CAT_SARCASTIC:
        return (uint32_t)BH_SARCASTIC_COUNT;
    case BH_CLI_CAT_INFORMATIONAL:
        return (uint32_t)BH_INFORMATIONAL_COUNT;
    default:
        return 0u;
    }
}

/* Quote at a zero-based index in a backed category, or NULL when the category is
 * not backed or the index is past the end of its table. */
static inline const char *bh_cli_quote_at(bh_cli_cat_t cat, uint32_t index) {
    if (cat == BH_CLI_CAT_SARCASTIC && index < (uint32_t)BH_SARCASTIC_COUNT) {
        return BH_SARCASTIC_QUOTES[index];
    }
    if (cat == BH_CLI_CAT_INFORMATIONAL &&
        index < (uint32_t)BH_INFORMATIONAL_COUNT) {
        return BH_INFORMATIONAL_QUOTES[index];
    }
    return NULL;
}

/* The quote the panic path would print for this seed.  NULL for a category that
 * has no table. */
static inline const char *bh_cli_select(bh_cli_cat_t cat, uint64_t seed) {
    if (cat == BH_CLI_CAT_SARCASTIC) {
        return bh_select_quote(seed, BH_TEXT_SARCASTIC);
    }
    if (cat == BH_CLI_CAT_INFORMATIONAL) {
        return bh_select_quote(seed, BH_TEXT_INFORMATIONAL);
    }
    return NULL;
}

/* Strict unsigned 64-bit parse: optional `0x`/`0X` for hexadecimal, otherwise
 * decimal; one or more digits, nothing else.  Returns false on empty input, a
 * lone/empty prefix, any non-digit, or overflow past UINT64_MAX.  Rejects signs
 * on purpose: a seed is a timestamp/fault CRC, never negative. */
static inline bool bh_cli_parse_u64(const char *text, uint64_t *out) {
    size_t i = 0u;
    uint32_t base = 10u;
    uint64_t acc = 0u;

    if (text == NULL || out == NULL) return false;

    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
        base = 16u;
        i = 2u;
    }
    if (text[i] == '\0') return false;

    for (; text[i] != '\0'; ++i) {
        uint64_t digit;
        char c = text[i];
        if (c >= '0' && c <= '9') {
            digit = (uint64_t)(c - '0');
        } else if (base == 16u && c >= 'a' && c <= 'f') {
            digit = (uint64_t)(c - 'a') + 10u;
        } else if (base == 16u && c >= 'A' && c <= 'F') {
            digit = (uint64_t)(c - 'A') + 10u;
        } else {
            return false;
        }
        if (digit >= (uint64_t)base) return false;
        if (acc > (UINT64_MAX - digit) / (uint64_t)base) return false;
        acc = acc * (uint64_t)base + digit;
    }

    *out = acc;
    return true;
}

#endif /* BANHAMMER_CLI_CORE_H */
