/*
 * kconfctl_core.h - pure logic for the OpenWindows configuration control CLI.
 *
 * Split out of kconfctl.c so the query/set/parse/validation rules can be driven
 * by a host test with no kernel, no trap and no console: nothing in this header
 * performs I/O.  kconfctl.c supplies the argument handling and the UART output;
 * the host harness links these same functions against the same kconf64 library
 * the program statically links.
 *
 * C99 freestanding.  Zero dynamic heap allocation.
 */
#ifndef KCONFCTL_CORE_H
#define KCONFCTL_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../DLL/kconf64/kconf64.h"
#include "../../Extensions/kconf.h"

/* Sub-commands the CLI recognises.  NONE means "no verb was supplied" and is
 * kept distinct from UNKNOWN so "kconfctl" with no argument can print usage
 * while "kconfctl frobnicate" reports the offending verb. */
typedef enum {
    KCONFCTL_CMD_NONE = 0,
    KCONFCTL_CMD_HELP,
    KCONFCTL_CMD_GET,
    KCONFCTL_CMD_GETINT,
    KCONFCTL_CMD_SET,
    KCONFCTL_CMD_SETINT,
    KCONFCTL_CMD_COUNT,
    KCONFCTL_CMD_UNKNOWN
} kconfctl_cmd_t;

static inline size_t kconfctl_strlen(const char *text) {
    size_t n = 0u;
    if (text == NULL) {
        return 0u;
    }
    while (text[n] != '\0') {
        ++n;
    }
    return n;
}

static inline bool kconfctl_str_eq(const char *a, const char *b) {
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

static inline kconfctl_cmd_t kconfctl_classify(const char *verb) {
    if (verb == NULL) return KCONFCTL_CMD_NONE;
    if (kconfctl_str_eq(verb, "help"))   return KCONFCTL_CMD_HELP;
    if (kconfctl_str_eq(verb, "get"))    return KCONFCTL_CMD_GET;
    if (kconfctl_str_eq(verb, "getint")) return KCONFCTL_CMD_GETINT;
    if (kconfctl_str_eq(verb, "set"))    return KCONFCTL_CMD_SET;
    if (kconfctl_str_eq(verb, "setint")) return KCONFCTL_CMD_SETINT;
    if (kconfctl_str_eq(verb, "count"))  return KCONFCTL_CMD_COUNT;
    return KCONFCTL_CMD_UNKNOWN;
}

/* Fetch a string value.  Returns the stored string, or NULL when the key is
 * absent or holds a non-string value: kconf64_get_string returns the caller's
 * default in both cases, and NULL is used as that default here. */
static inline const char *kconfctl_query_string(const char *key) {
    if (key == NULL) return NULL;
    return kconf64_get_string(key, (const char *)0);
}

/* Fetch an integer value.  Returns true and stores it when the key holds an
 * integer; false when it is absent or non-integer.  kconf64 exposes no separate
 * "present" predicate, so INT64_MIN is used as the sentinel default; an integer
 * key whose stored value IS INT64_MIN would read back as absent.  The built-in
 * defaults are nowhere near it, and this is stated rather than hidden. */
static inline bool kconfctl_query_int(const char *key, int64_t *out) {
    int64_t v;
    if (key == NULL || out == NULL) return false;
    v = kconf64_get_int(key, INT64_MIN);
    if (v == INT64_MIN) return false;
    *out = v;
    return true;
}

/* Apply a string value.  Rejects a key that is not a valid C string, and a key
 * or value that would not fit the engine's fixed-width storage: kconf_set_str
 * silently truncates past KCONF_MAX_*_LEN, so the bound is enforced here where
 * it can be reported instead. */
static inline bool kconfctl_apply_string(const char *key, const char *val) {
    if (key == NULL || val == NULL) return false;
    if (kconfctl_strlen(key) >= (size_t)KCONF_MAX_KEY_LEN) return false;
    if (kconfctl_strlen(val) >= (size_t)KCONF_MAX_VAL_LEN) return false;
    return kconf64_set_string(key, val);
}

static inline bool kconfctl_apply_int(const char *key, int64_t val) {
    if (key == NULL) return false;
    if (kconfctl_strlen(key) >= (size_t)KCONF_MAX_KEY_LEN) return false;
    return kconf64_set_int(key, val);
}

/* Strict base-10 signed 64-bit parse: optional single '+'/'-', then one or more
 * digits, nothing else.  Returns false on empty input, a lone sign, any non
 * digit, or overflow past INT64_MIN/INT64_MAX.  Accumulated unsigned so the
 * INT64_MIN magnitude (2^63) is representable before the sign is applied. */
static inline bool kconfctl_parse_i64(const char *text, int64_t *out) {
    size_t i = 0u;
    bool neg = false;
    uint64_t acc = 0u;
    uint64_t limit;

    if (text == NULL || out == NULL) return false;

    if (text[i] == '+' || text[i] == '-') {
        neg = (text[i] == '-');
        ++i;
    }
    if (text[i] == '\0') return false;

    limit = neg ? ((uint64_t)1u << 63) : (((uint64_t)1u << 63) - 1u);

    for (; text[i] != '\0'; ++i) {
        uint64_t digit;
        if (text[i] < '0' || text[i] > '9') return false;
        digit = (uint64_t)(text[i] - '0');
        if (acc > (limit - digit) / 10u) return false;
        acc = acc * 10u + digit;
    }

    *out = neg ? (int64_t)((uint64_t)0u - acc) : (int64_t)acc;
    return true;
}

#endif /* KCONFCTL_CORE_H */
