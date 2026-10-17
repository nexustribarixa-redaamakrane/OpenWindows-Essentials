/*
 * owsettings.c - OpenWindows System Settings GUI (.owx)
 *
 * A headless model of the graphical settings panel.  Each setting has a name,
 * a value type (enum, integer, or string), and a validator; `get`, `set`, and
 * `list` drive the panel and the resulting table is printed.  `set` rejects a
 * value that fails its type's validation and says why.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWSETTINGS_VERSION "1.0.0"
#define OWSETTINGS_VAL_MAX 24u
#define OWSETTINGS_KEY_MAX 16u

typedef enum {
    OWSETTINGS_ENUM = 0,
    OWSETTINGS_INT,
    OWSETTINGS_STRING
} owsettings_type_t;

typedef struct {
    const char *key;
    owsettings_type_t type;
    char value[OWSETTINGS_VAL_MAX];
    char def[OWSETTINGS_VAL_MAX];
    int32_t min;
    int32_t max;
    const char *const *allowed;
    uint32_t allowed_count;
} owsettings_item_t;

static const char *const owsettings_themes[] = {
    "light", "dark", "high-contrast", NULL
};
static const char *const owsettings_resolutions[] = {
    "800x600", "1024x768", "1280x720", "1920x1080", NULL
};

static owsettings_item_t s_items[] = {
    { "theme",      OWSETTINGS_ENUM,   "dark",     "dark",     0,   0,   owsettings_themes,      3u },
    { "resolution", OWSETTINGS_ENUM,   "1024x768", "1024x768", 0,   0,   owsettings_resolutions, 4u },
    { "volume",     OWSETTINGS_INT,    "60",       "60",       0,   100, NULL,                   0u },
    { "timezone",   OWSETTINGS_STRING, "UTC",      "UTC",      0,   0,   NULL,                   0u }
};
#define OWSETTINGS_COUNT (sizeof(s_items) / sizeof(s_items[0]))

/* ── Forward declarations ─────────────────────────────────────── */

static int  owsettings_run(int argc, const char *const *argv);
static void owsettings_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owsettings_usage();
        return 1;
    }
    return owsettings_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owsettings_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void owsettings_copy(char *dst, uint32_t cap, const char *src)
{
    uint32_t i = 0u;
    while (i + 1u < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool owsettings_parse_i32(const char *s, int32_t *out)
{
    bool neg = false;
    int64_t v = 0;
    if (s == NULL || *s == '\0') {
        return false;
    }
    if (*s == '-') {
        neg = true;
        s++;
    }
    if (*s == '\0') {
        return false;
    }
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        v = v * 10 + (int64_t)(*s - '0');
        if (v > 1000000) {
            return false;
        }
        s++;
    }
    *out = (int32_t)(neg ? -v : v);
    return true;
}

static void owsettings_pad(const char *s, uint32_t width)
{
    uint32_t n = 0u;
    while (*s != '\0' && n < width) {
        owx_putc(*s++);
        n++;
    }
    while (n < width) {
        owx_putc(' ');
        n++;
    }
}

static const char *owsettings_type_name(owsettings_type_t t)
{
    switch (t) {
    case OWSETTINGS_ENUM:
        return "enum";
    case OWSETTINGS_INT:
        return "int";
    case OWSETTINGS_STRING:
        return "string";
    default:
        return "?";
    }
}

static owsettings_item_t *owsettings_find(const char *key)
{
    uint32_t i;
    for (i = 0u; i < OWSETTINGS_COUNT; i++) {
        if (owsettings_streq(s_items[i].key, key)) {
            return &s_items[i];
        }
    }
    return NULL;
}

static bool owsettings_enum_ok(const owsettings_item_t *it, const char *v)
{
    uint32_t i;
    for (i = 0u; i < it->allowed_count; i++) {
        if (owsettings_streq(it->allowed[i], v)) {
            return true;
        }
    }
    return false;
}

/* A timezone is a short identifier: letters, digits, and / + - _ : */
static bool owsettings_string_ok(const char *v)
{
    uint32_t n = 0u;
    if (v == NULL || *v == '\0') {
        return false;
    }
    while (*v != '\0') {
        char c = *v;
        bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                  (c >= '0' && c <= '9') || c == '/' || c == '+' ||
                  c == '-' || c == '_' || c == ':';
        if (!ok) {
            return false;
        }
        n++;
        if (n >= OWSETTINGS_VAL_MAX) {
            return false;
        }
        v++;
    }
    return true;
}

static void owsettings_list(void)
{
    uint32_t i;
    owx_puts("owsettings: settings table (");
    owx_put_u64((uint64_t)OWSETTINGS_COUNT);
    owx_puts(" entries)\r\n");
    owx_puts("  key          type    value\r\n");
    for (i = 0u; i < OWSETTINGS_COUNT; i++) {
        owx_puts("  ");
        owsettings_pad(s_items[i].key, 12u);
        owsettings_pad(owsettings_type_name(s_items[i].type), 8u);
        owx_puts(s_items[i].value);
        owx_puts("\r\n");
    }
}

static void owsettings_get(const char *key)
{
    owsettings_item_t *it = owsettings_find(key);
    if (it == NULL) {
        owx_puts("owsettings: unknown setting '");
        owx_puts(key);
        owx_puts("'\r\n");
        return;
    }
    owx_puts("owsettings: ");
    owx_puts(it->key);
    owx_puts(" = ");
    owx_puts(it->value);
    owx_puts(" (");
    owx_puts(owsettings_type_name(it->type));
    if (it->type == OWSETTINGS_INT) {
        owx_puts(", ");
        owx_put_i64((int64_t)it->min);
        owx_puts("..");
        owx_put_i64((int64_t)it->max);
    }
    owx_puts(")\r\n");
}

/* Returns true if the value was applied. */
static bool owsettings_set(const char *key, const char *value)
{
    owsettings_item_t *it = owsettings_find(key);
    if (it == NULL) {
        owx_puts("owsettings: unknown setting '");
        owx_puts(key);
        owx_puts("'\r\n");
        return false;
    }
    if (value == NULL) {
        owx_puts("owsettings: set requires a value\r\n");
        return false;
    }

    if (it->type == OWSETTINGS_ENUM) {
        if (!owsettings_enum_ok(it, value)) {
            uint32_t i;
            owx_puts("owsettings: '");
            owx_puts(value);
            owx_puts("' is not valid for ");
            owx_puts(key);
            owx_puts(" (allowed:");
            for (i = 0u; i < it->allowed_count; i++) {
                owx_putc(' ');
                owx_puts(it->allowed[i]);
            }
            owx_puts(")\r\n");
            return false;
        }
    } else if (it->type == OWSETTINGS_INT) {
        int32_t v = 0;
        if (!owsettings_parse_i32(value, &v)) {
            owx_puts("owsettings: ");
            owx_puts(key);
            owx_puts(" expects an integer\r\n");
            return false;
        }
        if (v < it->min || v > it->max) {
            owx_puts("owsettings: ");
            owx_puts(key);
            owx_puts(" must be within ");
            owx_put_i64((int64_t)it->min);
            owx_puts("..");
            owx_put_i64((int64_t)it->max);
            owx_puts("\r\n");
            return false;
        }
    } else {
        if (!owsettings_string_ok(value)) {
            owx_puts("owsettings: '");
            owx_puts(value);
            owx_puts("' is not a valid ");
            owx_puts(key);
            owx_puts("\r\n");
            return false;
        }
    }

    owsettings_copy(it->value, OWSETTINGS_VAL_MAX, value);
    owx_puts("owsettings: ");
    owx_puts(key);
    owx_puts(" := ");
    owx_puts(it->value);
    owx_puts("\r\n");
    return true;
}

/* ── Implementation ───────────────────────────────────────────── */

static int owsettings_run(int argc, const char *const *argv)
{
    int i;
    int rc = 0;

    if (argc <= 1) {
        owsettings_list();
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const char *cmd = argv[i];
        if (cmd == NULL) {
            continue;
        }
        if (owsettings_streq(cmd, "list")) {
            owsettings_list();
        } else if (owsettings_streq(cmd, "get")) {
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                owx_puts("owsettings: get requires a key\r\n");
                rc = 2;
                continue;
            }
            owsettings_get(argv[++i]);
        } else if (owsettings_streq(cmd, "set")) {
            const char *key;
            const char *value;
            if (i + 2 >= argc || argv[i + 1] == NULL || argv[i + 2] == NULL) {
                owx_puts("owsettings: set requires a key and a value\r\n");
                rc = 2;
                continue;
            }
            key = argv[i + 1];
            value = argv[i + 2];
            i += 2;
            if (!owsettings_set(key, value)) {
                rc = 2;
            }
        } else if (owsettings_streq(cmd, "reset")) {
            if (i + 1 < argc && argv[i + 1] != NULL) {
                const char *key = argv[++i];
                owsettings_item_t *it = owsettings_find(key);
                if (it == NULL) {
                    owx_puts("owsettings: unknown setting '");
                    owx_puts(key);
                    owx_puts("'\r\n");
                    rc = 2;
                } else {
                    owsettings_copy(it->value, OWSETTINGS_VAL_MAX, it->def);
                    owx_puts("owsettings: reset ");
                    owx_puts(it->key);
                    owx_puts(" -> ");
                    owx_puts(it->value);
                    owx_puts("\r\n");
                }
            } else {
                uint32_t k;
                owx_puts("owsettings: reset all settings\r\n");
                for (k = 0u; k < OWSETTINGS_COUNT; k++) {
                    owsettings_copy(s_items[k].value, OWSETTINGS_VAL_MAX,
                                    s_items[k].def);
                }
            }
        } else if (owsettings_streq(cmd, "-h") || owsettings_streq(cmd, "--help")) {
            owsettings_usage();
            return 0;
        } else {
            owx_puts("owsettings: unknown command '");
            owx_puts(cmd);
            owx_puts("'\r\n");
            owsettings_usage();
            return 2;
        }
    }

    owsettings_list();
    return rc;
}

static void owsettings_usage(void)
{
    owx_puts(
        "usage: owsettings [command ...]\r\n"
        "  list                 print the settings table\r\n"
        "  get <key>            print one setting and its type\r\n"
        "  set <key> <value>    validate and apply a value\r\n"
        "  reset [key]          restore one setting, or all, to defaults\r\n"
        "  keys: theme (enum) resolution (enum) volume (int) timezone (string)\r\n");
}
