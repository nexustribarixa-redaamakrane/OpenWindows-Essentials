/*
 * owinsmod.c - OpenWindows Module Installer (.owx)
 *
 * Installs a .owc kernel module.  The Ring 3 gateway has no module-load
 * syscall, so installation updates an in-process model of the kernel module
 * table: the image name is validated (must be a non-empty, well-formed .owc
 * stem), rejected if already resident or if the table is full, and otherwise
 * inserted with a modeled image size and a zero reference count.  The
 * remaining arguments are recorded as the module's load parameters.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWINSMOD_VERSION     "1.0.0"
#define OWINSMOD_MAX_MODULES 16u
#define OWINSMOD_NAME_MAX    32u
#define OWINSMOD_BASE_SIZE   32768u
#define OWINSMOD_ARG_SIZE    4096u

/* ── Module table model ───────────────────────────────────────── */

typedef struct {
    char     name[OWINSMOD_NAME_MAX];
    uint64_t size;
    uint32_t refcount;
} owinsmod_mod_t;

typedef struct {
    const char *name;
    uint64_t    size;
    uint32_t    refcount;
} owinsmod_seed_t;

static const owinsmod_seed_t owinsmod_seed[] = {
    { "fltrmgr",   28672u, 4u },
    { "classpnp",  40960u, 2u },
    { "owfs",      65536u, 3u },
    { "bootvid",   16384u, 0u }
};

static owinsmod_mod_t s_mods[OWINSMOD_MAX_MODULES];
static uint32_t       s_nmods;

/* ── Tiny string / number helpers ─────────────────────────────── */

static uint32_t owinsmod_strlen(const char *s)
{
    uint32_t n = 0u;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static bool owinsmod_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static char owinsmod_lower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static bool owinsmod_streq_ci(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0' && owinsmod_lower(*a) == owinsmod_lower(*b)) {
        a++;
        b++;
    }
    return owinsmod_lower(*a) == owinsmod_lower(*b);
}

/* Copy at most cap-1 bytes and always NUL-terminate. */
static void owinsmod_copy(char *dst, uint32_t cap, const char *src)
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

static const char *owinsmod_basename(const char *path)
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

static bool owinsmod_ends_with_ci(const char *s, const char *suffix)
{
    uint32_t slen = owinsmod_strlen(s);
    uint32_t xlen = owinsmod_strlen(suffix);
    uint32_t i;
    if (xlen > slen) {
        return false;
    }
    for (i = 0u; i < xlen; i++) {
        if (owinsmod_lower(s[slen - xlen + i]) != owinsmod_lower(suffix[i])) {
            return false;
        }
    }
    return true;
}

/* Module stems may hold letters, digits, '_' and '-' only. */
static bool owinsmod_name_ok(const char *stem)
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

static void owinsmod_init(void)
{
    uint32_t i;
    const uint32_t n = (uint32_t)(sizeof(owinsmod_seed) / sizeof(owinsmod_seed[0]));
    s_nmods = 0u;
    for (i = 0u; i < n && i < OWINSMOD_MAX_MODULES; i++) {
        owinsmod_copy(s_mods[i].name, OWINSMOD_NAME_MAX, owinsmod_seed[i].name);
        s_mods[i].size = owinsmod_seed[i].size;
        s_mods[i].refcount = owinsmod_seed[i].refcount;
        s_nmods++;
    }
}

static bool owinsmod_loaded(const char *stem)
{
    uint32_t i;
    for (i = 0u; i < s_nmods; i++) {
        if (owinsmod_streq_ci(s_mods[i].name, stem)) {
            return true;
        }
    }
    return false;
}

/* ── Forward declarations ─────────────────────────────────────── */

static int  owinsmod_run(int argc, const char *const *argv);
static void owinsmod_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owinsmod_usage();
        return 1;
    }
    return owinsmod_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owinsmod_run(int argc, const char *const *argv)
{
    const char *spec = NULL;
    const char *params[8];
    uint32_t     nparams = 0u;
    int          ai;
    const char  *base;
    char         stem[OWINSMOD_NAME_MAX];
    uint32_t     stemlen;
    uint64_t     size;
    uint32_t     i;

    owinsmod_init();

    for (ai = 1; ai < argc; ai++) {
        const char *a = argv[ai];
        if (a == NULL) {
            continue;
        }
        if (a[0] == '-' && a[1] != '\0' && spec == NULL) {
            if (owinsmod_streq(a, "--help")) {
                owinsmod_usage();
                return 0;
            }
            owx_puts("owinsmod: invalid option '");
            owx_puts(a);
            owx_puts("'\r\n");
            owinsmod_usage();
            return 1;
        }
        if (spec == NULL) {
            spec = a;
        } else if (nparams < 8u) {
            params[nparams++] = a;
        }
    }

    if (spec == NULL) {
        owinsmod_usage();
        return 1;
    }

    base = owinsmod_basename(spec);
    if (!owinsmod_ends_with_ci(base, ".owc")) {
        owx_puts("owinsmod: '");
        owx_puts(spec);
        owx_puts("': not a .owc module image\r\n");
        return 1;
    }

    stemlen = owinsmod_strlen(base) - 4u;   /* drop ".owc" */
    if (stemlen == 0u || stemlen >= OWINSMOD_NAME_MAX) {
        owx_puts("owinsmod: module name is too long or empty\r\n");
        return 1;
    }
    for (i = 0u; i < stemlen; i++) {
        stem[i] = base[i];
    }
    stem[stemlen] = '\0';

    if (!owinsmod_name_ok(stem)) {
        owx_puts("owinsmod: module name '");
        owx_puts(stem);
        owx_puts("' contains invalid characters\r\n");
        return 1;
    }
    if (owinsmod_loaded(stem)) {
        owx_puts("owinsmod: module '");
        owx_puts(stem);
        owx_puts("' is already loaded\r\n");
        return 1;
    }
    if (s_nmods >= OWINSMOD_MAX_MODULES) {
        owx_puts("owinsmod: module table is full\r\n");
        return 1;
    }

    size = (uint64_t)OWINSMOD_BASE_SIZE + (uint64_t)nparams * (uint64_t)OWINSMOD_ARG_SIZE;
    owinsmod_copy(s_mods[s_nmods].name, OWINSMOD_NAME_MAX, stem);
    s_mods[s_nmods].size = size;
    s_mods[s_nmods].refcount = 0u;
    s_nmods++;

    owx_puts("owinsmod: inserted ");
    owx_puts(stem);
    owx_puts(" size=");
    owx_put_u64(size);
    owx_puts(" refcount=0\r\n");

    for (i = 0u; i < nparams; i++) {
        owx_puts("owinsmod:   param ");
        owx_put_u64((uint64_t)(i + 1u));
        owx_puts(": ");
        owx_puts(params[i]);
        owx_puts("\r\n");
    }

    owx_puts("owinsmod: ");
    owx_put_u64((uint64_t)s_nmods);
    owx_puts(" module(s) loaded\r\n");
    return 0;
}

static void owinsmod_usage(void)
{
    owx_puts("usage: owinsmod <module.owc> [param ...]\r\n");
    owx_puts("  installs a .owc module into the kernel module table\r\n");
    owx_puts("  version " OWINSMOD_VERSION "\r\n");
}
