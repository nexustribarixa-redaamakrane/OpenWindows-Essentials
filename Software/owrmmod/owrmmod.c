/*
 * owrmmod.c - OpenWindows Module Remover (.owx)
 *
 * Removes a .owc module from the kernel module table.  The Ring 3 gateway has
 * no module-unload syscall, so the operation runs against an in-process model
 * of the module table: the module must be resident and, unless -f is given, its
 * reference count must be zero, mirroring the kernel's own "module in use"
 * refusal.  A successful removal compacts the table and reports the new count.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWRMMOD_VERSION      "1.0.0"
#define OWRMMOD_MAX_MODULES  16u
#define OWRMMOD_NAME_MAX     32u
#define OWRMMOD_NOT_FOUND    0xFFFFFFFFu

/* ── Module table model ───────────────────────────────────────── */

typedef struct {
    char     name[OWRMMOD_NAME_MAX];
    uint32_t refcount;
} owrmmod_mod_t;

typedef struct {
    const char *name;
    uint32_t    refcount;
} owrmmod_seed_t;

static const owrmmod_seed_t owrmmod_seed[] = {
    { "fltrmgr",   4u },
    { "classpnp",  2u },
    { "owfs",      3u },
    { "bootvid",   0u },
    { "usfs",      0u }
};

static owrmmod_mod_t s_mods[OWRMMOD_MAX_MODULES];
static uint32_t      s_nmods;

/* ── Tiny string helpers ──────────────────────────────────────── */

static uint32_t owrmmod_strlen(const char *s)
{
    uint32_t n = 0u;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static bool owrmmod_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static char owrmmod_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static bool owrmmod_streq_ci(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0' && owrmmod_lower(*a) == owrmmod_lower(*b)) {
        a++;
        b++;
    }
    return owrmmod_lower(*a) == owrmmod_lower(*b);
}

static void owrmmod_copy(char *dst, uint32_t cap, const char *src)
{
    uint32_t i = 0u;
    if (cap == 0u) {
        return;
    }
    while (src[i] != '\0' && i + 1u < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static const char *owrmmod_basename(const char *path)
{
    const char *base = path;
    const char *p = path;
    while (*p != '\0') {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
        p++;
    }
    return base;
}

static bool owrmmod_ends_with_ci(const char *s, const char *suffix)
{
    uint32_t slen = owrmmod_strlen(s);
    uint32_t xlen = owrmmod_strlen(suffix);
    uint32_t i;
    if (xlen > slen) {
        return false;
    }
    for (i = 0u; i < xlen; i++) {
        if (owrmmod_lower(s[slen - xlen + i]) != owrmmod_lower(suffix[i])) {
            return false;
        }
    }
    return true;
}

static bool owrmmod_name_ok(const char *stem)
{
    if (stem[0] == '\0') {
        return false;
    }
    while (*stem != '\0') {
        char c = *stem;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
        stem++;
    }
    return true;
}

static void owrmmod_init(void)
{
    uint32_t i;
    const uint32_t n = (uint32_t)(sizeof(owrmmod_seed) / sizeof(owrmmod_seed[0]));
    s_nmods = 0u;
    for (i = 0u; i < n && i < OWRMMOD_MAX_MODULES; i++) {
        owrmmod_copy(s_mods[i].name, OWRMMOD_NAME_MAX, owrmmod_seed[i].name);
        s_mods[i].refcount = owrmmod_seed[i].refcount;
        s_nmods++;
    }
}

static uint32_t owrmmod_find(const char *stem)
{
    uint32_t i;
    for (i = 0u; i < s_nmods; i++) {
        if (owrmmod_streq_ci(s_mods[i].name, stem)) {
            return i;
        }
    }
    return OWRMMOD_NOT_FOUND;
}

/* ── Forward declarations ─────────────────────────────────────── */

static int  owrmmod_run(int argc, const char *const *argv);
static void owrmmod_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owrmmod_usage();
        return 1;
    }
    return owrmmod_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owrmmod_run(int argc, const char *const *argv)
{
    const char *spec = NULL;
    bool        force = false;
    int         ai;
    const char *base;
    char        stem[OWRMMOD_NAME_MAX];
    uint32_t    stemlen;
    uint32_t    idx;
    uint32_t    i;

    owrmmod_init();

    for (ai = 1; ai < argc; ai++) {
        const char *a = argv[ai];
        if (a == NULL) {
            continue;
        }
        if (a[0] == '-' && a[1] != '\0') {
            if (owrmmod_streq(a, "--help")) {
                owrmmod_usage();
                return 0;
            }
            if (owrmmod_streq(a, "-f")) {
                force = true;
                continue;
            }
            owx_puts("owrmmod: invalid option '");
            owx_puts(a);
            owx_puts("'\r\n");
            owrmmod_usage();
            return 1;
        }
        if (spec == NULL) {
            spec = a;
        }
    }

    if (spec == NULL) {
        owrmmod_usage();
        return 1;
    }

    base = owrmmod_basename(spec);
    stemlen = owrmmod_strlen(base);
    if (owrmmod_ends_with_ci(base, ".owc")) {
        stemlen -= 4u;
    }
    if (stemlen == 0u || stemlen >= OWRMMOD_NAME_MAX) {
        owx_puts("owrmmod: invalid module name\r\n");
        return 1;
    }
    for (i = 0u; i < stemlen; i++) {
        stem[i] = base[i];
    }
    stem[stemlen] = '\0';

    if (!owrmmod_name_ok(stem)) {
        owx_puts("owrmmod: invalid module name '");
        owx_puts(stem);
        owx_puts("'\r\n");
        return 1;
    }

    idx = owrmmod_find(stem);
    if (idx == OWRMMOD_NOT_FOUND) {
        owx_puts("owrmmod: module '");
        owx_puts(stem);
        owx_puts("' is not loaded\r\n");
        return 1;
    }

    {
        uint32_t removed_ref = s_mods[idx].refcount;

        if (removed_ref > 0u && !force) {
            owx_puts("owrmmod: module '");
            owx_puts(stem);
            owx_puts("' is in use (refcount=");
            owx_put_u64((uint64_t)removed_ref);
            owx_puts(")\r\n");
            return 1;
        }

        /* Compact the table so no hole is left behind. */
        for (i = idx; i + 1u < s_nmods; i++) {
            s_mods[i] = s_mods[i + 1u];
        }
        if (s_nmods > 0u) {
            s_nmods--;
        }

        owx_puts("owrmmod: removed ");
        owx_puts(stem);
        if (force && removed_ref > 0u) {
            owx_puts(" (forced)");
        }
        owx_puts("\r\n");
    }
    owx_puts("owrmmod: ");
    owx_put_u64((uint64_t)s_nmods);
    owx_puts(" module(s) loaded\r\n");
    return 0;
}

static void owrmmod_usage(void)
{
    owx_puts("usage: owrmmod [-f] [--help] <module>\r\n");
    owx_puts("  removes the named module from the kernel module table\r\n");
    owx_puts("  -f  force removal even if the module is in use\r\n");
    owx_puts("  version " OWRMMOD_VERSION "\r\n");
}
