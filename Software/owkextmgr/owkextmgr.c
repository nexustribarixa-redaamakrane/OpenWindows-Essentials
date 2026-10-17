/*
 * owkextmgr.c - OpenWindows Kernel Extension Manager (.owx)
 *
 * Lists, loads and unloads the modelled kernel extensions (kexts).  Each entry
 * carries a version, the image path, an optional hard dependency, a reference
 * count and a loaded flag.  `load` enforces the dependency and refuses a kext
 * whose image is not marked present; `unload` refuses a kext still referenced.
 *
 * The kext table is a static model.  The Ring 3 gateway in owx_gate.h has no
 * module call -- a real load is a Ring 0 operation -- so state changes are
 * applied here and reported as in-memory.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWKEXTMGR_VERSION "1.0.0"
#define OWKEXTMGR_MAX 16u
#define OWKEXTMGR_NAME_MAX 24u
#define OWKEXTMGR_PATH_MAX 48u

/* ── Kernel extension model ───────────────────────────────────── */

typedef struct {
    char name[OWKEXTMGR_NAME_MAX];
    char version[16];
    char path[OWKEXTMGR_PATH_MAX];
    const char *depends; /* required kext name, or NULL */
    bool present;        /* image found on the boot volume */
    bool loaded;
    bool builtin;        /* statically linked, cannot unload */
    uint32_t refs;
} owkext_entry_t;

static owkext_entry_t s_kexts[OWKEXTMGR_MAX];
static uint32_t s_kext_count;

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owkextmgr_streq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static void owkextmgr_copy(char *dst, uint32_t cap, const char *src)
{
    uint32_t i = 0u;
    while (src[i] != '\0' && i + 1u < cap) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static void owkextmgr_add(const char *name, const char *version, const char *path,
                          const char *depends, bool present, bool loaded,
                          bool builtin, uint32_t refs)
{
    owkext_entry_t *k;
    if (s_kext_count >= OWKEXTMGR_MAX) {
        return;
    }
    k = &s_kexts[s_kext_count++];
    owkextmgr_copy(k->name, OWKEXTMGR_NAME_MAX, name);
    owkextmgr_copy(k->version, 16u, version);
    owkextmgr_copy(k->path, OWKEXTMGR_PATH_MAX, path);
    k->depends = depends;
    k->present = present;
    k->loaded = loaded;
    k->builtin = builtin;
    k->refs = refs;
}

static void owkextmgr_seed(void)
{
    s_kext_count = 0u;
    owkextmgr_add("htl",      "1.0.0", "\\System\\htl.owc",      NULL,      true, true,  true,  1u);
    owkextmgr_add("classpnp", "1.0.0", "\\System\\classpnp.owc", "htl",     true, true,  false, 1u);
    owkextmgr_add("fltrmgr",  "1.0.0", "\\System\\fltrmgr.owc",  "htl",     true, true,  false, 2u);
    owkextmgr_add("bootvid",  "1.0.0", "\\System\\bootvid.owd",  NULL,      true, true,  true,  1u);
    owkextmgr_add("owfs",     "1.0.0", "\\System\\owfs.owc",     "classpnp", true, true, false, 3u);
    owkextmgr_add("vip",      "1.0.0", "\\System\\vip.owc",      "owfs",    true, false, false, 0u);
    owkextmgr_add("sufs",     "0.9.0", "\\System\\sufs.owc",     NULL,      true, false, false, 0u);
    owkextmgr_add("owrp",     "1.0.0", "\\System\\owrp.owc",     NULL,      false, false, false, 0u);
}

static owkext_entry_t *owkextmgr_find(const char *name)
{
    for (uint32_t i = 0u; i < s_kext_count; i++) {
        if (owkextmgr_streq(s_kexts[i].name, name)) {
            return &s_kexts[i];
        }
    }
    return NULL;
}

/* A dependency is satisfied only when it is present and already loaded. */
static bool owkextmgr_dep_ready(const owkext_entry_t *k)
{
    const owkext_entry_t *dep;
    if (k->depends == NULL) {
        return true;
    }
    dep = owkextmgr_find(k->depends);
    return dep != NULL && dep->loaded;
}

static void owkextmgr_text(const char *s, uint32_t width)
{
    uint32_t n = 0u;
    while (s[n] != '\0' && n < width) {
        owx_putc(s[n]);
        n++;
    }
    while (n < width) {
        owx_putc(' ');
        n++;
    }
}

static void owkextmgr_list(void)
{
    owkextmgr_text("NAME", 14u);
    owkextmgr_text("VERSION", 10u);
    owkextmgr_text("STATE", 10u);
    owkextmgr_text("REFS", 6u);
    owkextmgr_text("DEPENDS", 12u);
    owkextmgr_text("PATH", 24u);
    owx_puts("\r\n");
    for (uint32_t i = 0u; i < s_kext_count; i++) {
        const owkext_entry_t *k = &s_kexts[i];
        const char *state = k->loaded ? (k->builtin ? "builtin" : "loaded") : "unloaded";
        owkextmgr_text(k->name, 14u);
        owkextmgr_text(k->version, 10u);
        owkextmgr_text(state, 10u);
        owx_put_u64((uint64_t)k->refs);
        owkextmgr_text("", (k->refs < 10u) ? 5u : 4u);
        owkextmgr_text(k->depends != NULL ? k->depends : "-", 12u);
        owx_puts(k->path);
        owx_puts("\r\n");
    }
    owx_puts("owkextmgr: ");
    owx_put_u64((uint64_t)s_kext_count);
    owx_puts(" extension(s)\r\n");
}

/* ── Entry point ──────────────────────────────────────────────── */

static int  owkextmgr_run(int argc, const char *const *argv);
static void owkextmgr_usage(void);

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owkextmgr_usage();
        return 1;
    }
    return owkextmgr_run(argc, argv);
}

/* ── Implementation ───────────────────────────────────────────── */

static int owkextmgr_run(int argc, const char *const *argv)
{
    const char *verb;

    owkextmgr_seed();

    if (argv == NULL || argc < 2 || argv[1] == NULL ||
        owkextmgr_streq(argv[1], "list")) {
        owkextmgr_list();
        return 0;
    }

    verb = argv[1];

    if (owkextmgr_streq(verb, "-h") || owkextmgr_streq(verb, "--help")) {
        owkextmgr_usage();
        return 0;
    }

    if (owkextmgr_streq(verb, "status")) {
        owkext_entry_t *k;
        if (argc < 3 || argv[2] == NULL) {
            owx_puts("owkextmgr: status needs a name\r\n");
            owkextmgr_usage();
            return 1;
        }
        k = owkextmgr_find(argv[2]);
        if (k == NULL) {
            owx_puts("owkextmgr: no such extension: ");
            owx_puts(argv[2]);
            owx_puts("\r\n");
            return 1;
        }
        owx_puts("owkextmgr: ");
        owx_puts(k->name);
        owx_puts(" version=");
        owx_puts(k->version);
        owx_puts(k->loaded ? " loaded" : " unloaded");
        owx_puts(" refs=");
        owx_put_u64((uint64_t)k->refs);
        owx_puts(" present=");
        owx_puts(k->present ? "yes" : "no");
        owx_puts("\r\n");
        return 0;
    }

    if (owkextmgr_streq(verb, "load")) {
        owkext_entry_t *k;
        if (argc < 3 || argv[2] == NULL) {
            owx_puts("owkextmgr: load needs a name\r\n");
            owkextmgr_usage();
            return 1;
        }
        k = owkextmgr_find(argv[2]);
        if (k == NULL) {
            owx_puts("owkextmgr: no such extension: ");
            owx_puts(argv[2]);
            owx_puts("\r\n");
            return 1;
        }
        if (k->loaded) {
            owx_puts("owkextmgr: ");
            owx_puts(k->name);
            owx_puts(": already loaded\r\n");
            return 0;
        }
        if (!k->present) {
            owx_puts("owkextmgr: ");
            owx_puts(k->name);
            owx_puts(": image not present\r\n");
            return 1;
        }
        if (!owkextmgr_dep_ready(k)) {
            owx_puts("owkextmgr: ");
            owx_puts(k->name);
            owx_puts(": unresolved dependency ");
            owx_puts(k->depends != NULL ? k->depends : "(none)");
            owx_puts("\r\n");
            return 1;
        }
        k->loaded = true;
        k->refs = 1u;
        owx_puts("owkextmgr: loaded ");
        owx_puts(k->name);
        owx_puts(" (in-memory)\r\n");
        return 0;
    }

    if (owkextmgr_streq(verb, "unload")) {
        owkext_entry_t *k;
        if (argc < 3 || argv[2] == NULL) {
            owx_puts("owkextmgr: unload needs a name\r\n");
            owkextmgr_usage();
            return 1;
        }
        k = owkextmgr_find(argv[2]);
        if (k == NULL) {
            owx_puts("owkextmgr: no such extension: ");
            owx_puts(argv[2]);
            owx_puts("\r\n");
            return 1;
        }
        if (!k->loaded) {
            owx_puts("owkextmgr: ");
            owx_puts(k->name);
            owx_puts(": not loaded\r\n");
            return 0;
        }
        if (k->builtin) {
            owx_puts("owkextmgr: ");
            owx_puts(k->name);
            owx_puts(": built-in extension cannot be unloaded\r\n");
            return 1;
        }
        if (k->refs > 0u) {
            owx_puts("owkextmgr: ");
            owx_puts(k->name);
            owx_puts(": busy, refs=");
            owx_put_u64((uint64_t)k->refs);
            owx_puts("\r\n");
            return 1;
        }
        k->loaded = false;
        owx_puts("owkextmgr: unloaded ");
        owx_puts(k->name);
        owx_puts(" (in-memory)\r\n");
        return 0;
    }

    owx_puts("owkextmgr: unknown command: ");
    owx_puts(verb);
    owx_puts("\r\n");
    owkextmgr_usage();
    return 1;
}

static void owkextmgr_usage(void)
{
    owx_puts("owkextmgr " OWKEXTMGR_VERSION " - OpenWindows kernel extensions\r\n");
    owx_puts("usage: owkextmgr list            list every extension\r\n");
    owx_puts("       owkextmgr status <name>  show one extension\r\n");
    owx_puts("       owkextmgr load <name>    load an extension\r\n");
    owx_puts("       owkextmgr unload <name>  unload an extension\r\n");
}
