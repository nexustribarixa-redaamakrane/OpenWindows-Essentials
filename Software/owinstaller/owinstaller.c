/*
 * owinstaller.c - OpenWindows Package Installer GUI (.owx)
 *
 * A headless model of the graphical package installer.  It parses a
 * line-oriented manifest (name, version, dependency list, and the file list
 * with declared checksums), resolves dependencies against a modelled installed
 * set, lays out an install plan, and walks the plan with a text progress
 * display.  With no manifest argument a built-in one is used.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWINSTALLER_VERSION "1.0.0"

#define OWINSTALLER_NAME_MAX 32u
#define OWINSTALLER_VER_MAX  16u
#define OWINSTALLER_PATH_MAX 64u
#define OWINSTALLER_LINE_MAX 160u
#define OWINSTALLER_FILE_MAX 8192u
#define OWINSTALLER_MAX_DEPS  8u
#define OWINSTALLER_MAX_FILES 16u

typedef struct {
    char name[OWINSTALLER_NAME_MAX];
    char min[OWINSTALLER_VER_MAX];
} owinstaller_dep_t;

typedef struct {
    char     path[OWINSTALLER_PATH_MAX];
    uint32_t size;
    uint32_t sum;
} owinstaller_file_t;

typedef struct {
    char               name[OWINSTALLER_NAME_MAX];
    char               version[OWINSTALLER_VER_MAX];
    owinstaller_dep_t  deps[OWINSTALLER_MAX_DEPS];
    uint32_t           dep_count;
    owinstaller_file_t files[OWINSTALLER_MAX_FILES];
    uint32_t           file_count;
} owinstaller_manifest_t;

typedef struct {
    const char *name;
    const char *version;
} owinstaller_pkg_t;

/* What the modelled system already has installed. */
static const owinstaller_pkg_t owinstaller_installed[] = {
    { "htl",          "1.2.0" },
    { "owrp",         "1.0.3" },
    { "superunicode", "2.0.0" },
    { "bancat",       "1.1.0" },
    { "bootvid",      "1.0.0" }
};
#define OWINSTALLER_INSTALLED_COUNT \
    (sizeof(owinstaller_installed) / sizeof(owinstaller_installed[0]))

/* Fallback manifest, parsed through exactly the same reader. */
static const char owinstaller_default_manifest[] =
    "# default modelled OpenWindows package\n"
    "name owsample\n"
    "version 2.1.0\n"
    "dep htl 1.0.0\n"
    "dep owrp 0.9.5\n"
    "file \\System\\Programs\\owsample.owx 4096 deadbeef\n"
    "file \\System\\Config\\owsample.kconf 128 0011a2ff\n"
    "file \\System\\Documentation\\owsample\\readme.txt 2048 5a5a5a5a\n";

static char s_file[OWINSTALLER_FILE_MAX];

/* ── Forward declarations ─────────────────────────────────────── */

static int  owinstaller_run(int argc, const char *const *argv);
static void owinstaller_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owinstaller_usage();
        return 1;
    }
    return owinstaller_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owinstaller_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool owinstaller_is_space(char c)
{
    return (c == ' ' || c == '\t') ? true : false;
}

static void owinstaller_copy(char *dst, uint32_t cap, const char *src)
{
    uint32_t i = 0u;
    while (i + 1u < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool owinstaller_parse_u32(const char *s, uint32_t *out)
{
    uint64_t v = 0u;
    if (s == NULL || *s == '\0') {
        return false;
    }
    while (*s != '\0') {
        if (*s < '0' || *s > '9') {
            return false;
        }
        v = v * 10u + (uint64_t)(uint32_t)(*s - '0');
        if (v > 4294967295u) {
            return false;
        }
        s++;
    }
    *out = (uint32_t)v;
    return true;
}

static bool owinstaller_parse_hex(const char *s, uint32_t *out)
{
    uint32_t v = 0u;
    bool any = false;
    if (s == NULL) {
        return false;
    }
    while (*s != '\0') {
        uint32_t d;
        char c = *s;
        if (c >= '0' && c <= '9') {
            d = (uint32_t)(c - '0');
        } else if (c >= 'a' && c <= 'f') {
            d = (uint32_t)(c - 'a') + 10u;
        } else if (c >= 'A' && c <= 'F') {
            d = (uint32_t)(c - 'A') + 10u;
        } else {
            return false;
        }
        v = v * 16u + d;
        any = true;
        s++;
    }
    *out = v;
    return any;
}

static void owinstaller_put_hex8(uint32_t v)
{
    static const char digits[] = "0123456789abcdef";
    int shift;
    owx_puts("0x");
    for (shift = 28; shift >= 0; shift -= 4) {
        owx_putc(digits[(v >> shift) & 0xFu]);
    }
}

/* Next whitespace-delimited token from *p; false at end of line. */
static bool owinstaller_token(char **p, char *out, uint32_t cap)
{
    char *s = *p;
    uint32_t n = 0u;
    while (*s != '\0' && owinstaller_is_space(*s)) {
        s++;
    }
    if (*s == '\0') {
        *p = s;
        return false;
    }
    while (*s != '\0' && !owinstaller_is_space(*s)) {
        if (n + 1u < cap) {
            out[n++] = *s;
        }
        s++;
    }
    out[n] = '\0';
    *p = s;
    return true;
}

/* Numeric version compare on up to four dot-separated components. */
static int owinstaller_vercmp(const char *a, const char *b)
{
    int part;
    for (part = 0; part < 4; part++) {
        uint32_t va = 0u;
        uint32_t vb = 0u;
        while (*a >= '0' && *a <= '9') {
            va = va * 10u + (uint32_t)(*a - '0');
            a++;
        }
        while (*b >= '0' && *b <= '9') {
            vb = vb * 10u + (uint32_t)(*b - '0');
            b++;
        }
        if (va != vb) {
            return (va < vb) ? -1 : 1;
        }
        if (*a == '.') {
            a++;
        }
        if (*b == '.') {
            b++;
        }
        if (*a == '\0' && *b == '\0') {
            break;
        }
    }
    return 0;
}

static void owinstaller_handle_line(char *line, owinstaller_manifest_t *m)
{
    char *p = line;
    char key[24];
    char a[OWINSTALLER_PATH_MAX];
    char b[OWINSTALLER_VER_MAX];
    char c[24];

    while (*p != '\0' && owinstaller_is_space(*p)) {
        p++;
    }
    if (*p == '\0' || *p == '#') {
        return;
    }
    if (!owinstaller_token(&p, key, (uint32_t)sizeof(key))) {
        return;
    }

    if (owinstaller_streq(key, "name")) {
        if (owinstaller_token(&p, a, (uint32_t)sizeof(a))) {
            owinstaller_copy(m->name, OWINSTALLER_NAME_MAX, a);
        }
    } else if (owinstaller_streq(key, "version")) {
        if (owinstaller_token(&p, a, (uint32_t)sizeof(a))) {
            owinstaller_copy(m->version, OWINSTALLER_VER_MAX, a);
        }
    } else if (owinstaller_streq(key, "dep")) {
        if (m->dep_count < OWINSTALLER_MAX_DEPS &&
            owinstaller_token(&p, a, (uint32_t)sizeof(a))) {
            owinstaller_copy(m->deps[m->dep_count].name, OWINSTALLER_NAME_MAX, a);
            m->deps[m->dep_count].min[0] = '\0';
            if (owinstaller_token(&p, b, (uint32_t)sizeof(b))) {
                owinstaller_copy(m->deps[m->dep_count].min, OWINSTALLER_VER_MAX, b);
            }
            m->dep_count++;
        }
    } else if (owinstaller_streq(key, "file")) {
        if (m->file_count < OWINSTALLER_MAX_FILES &&
            owinstaller_token(&p, a, (uint32_t)sizeof(a))) {
            uint32_t size = 0u;
            uint32_t sum = 0u;
            owinstaller_file_t *f = &m->files[m->file_count];
            owinstaller_copy(f->path, OWINSTALLER_PATH_MAX, a);
            f->size = 0u;
            f->sum = 0u;
            if (owinstaller_token(&p, b, (uint32_t)sizeof(b))) {
                (void)owinstaller_parse_u32(b, &size);
            }
            if (owinstaller_token(&p, c, (uint32_t)sizeof(c))) {
                (void)owinstaller_parse_hex(c, &sum);
            }
            f->size = size;
            f->sum = sum;
            m->file_count++;
        }
    }
}

static void owinstaller_parse(const char *buf, uint32_t len,
                              owinstaller_manifest_t *m)
{
    uint32_t i = 0u;
    while (i < len) {
        char line[OWINSTALLER_LINE_MAX];
        uint32_t n = 0u;
        while (i < len && buf[i] != '\n') {
            char c = buf[i];
            if (c == '\r') {
                c = ' ';
            }
            if (n + 1u < OWINSTALLER_LINE_MAX) {
                line[n++] = c;
            }
            i++;
        }
        if (i < len) {
            i++;
        }
        line[n] = '\0';
        owinstaller_handle_line(line, m);
    }
}

static void owinstaller_progress(uint32_t pct)
{
    uint32_t filled = (pct * 10u) / 100u;
    uint32_t k;
    if (filled > 10u) {
        filled = 10u;
    }
    owx_puts("  [");
    for (k = 0u; k < 10u; k++) {
        owx_putc((k < filled) ? '#' : '-');
    }
    owx_puts("] ");
    if (pct < 10u) {
        owx_puts("  ");
    } else if (pct < 100u) {
        owx_puts(" ");
    }
    owx_put_u64((uint64_t)pct);
    owx_puts("%\r\n");
}

/* ── Implementation ───────────────────────────────────────────── */

static int owinstaller_run(int argc, const char *const *argv)
{
    const char *path = NULL;
    bool dry_run = false;
    bool interactive = false;
    bool force = false;
    owinstaller_manifest_t m;
    uint32_t i;
    uint32_t satisfied = 0u;
    uint32_t total_bytes = 0u;
    uint32_t done_bytes = 0u;
    int a;

    for (a = 1; a < argc; a++) {
        const char *opt = argv[a];
        if (opt == NULL) {
            continue;
        }
        if (owinstaller_streq(opt, "-f") || owinstaller_streq(opt, "--file")) {
            if (a + 1 >= argc || argv[a + 1] == NULL) {
                owx_puts("owinstaller: -f requires a manifest path\r\n");
                owinstaller_usage();
                return 2;
            }
            path = argv[++a];
        } else if (owinstaller_streq(opt, "-n") || owinstaller_streq(opt, "--dry-run")) {
            dry_run = true;
        } else if (owinstaller_streq(opt, "-i") || owinstaller_streq(opt, "--interactive")) {
            interactive = true;
        } else if (owinstaller_streq(opt, "--force")) {
            force = true;
        } else if (owinstaller_streq(opt, "-h") || owinstaller_streq(opt, "--help")) {
            owinstaller_usage();
            return 0;
        } else {
            owx_puts("owinstaller: unknown option '");
            owx_puts(opt);
            owx_puts("'\r\n");
            owinstaller_usage();
            return 2;
        }
    }

    /* Zero the manifest, then parse whichever source we have. */
    {
        uint8_t *raw = (uint8_t *)&m;
        uint32_t k;
        for (k = 0u; k < (uint32_t)sizeof(m); k++) {
            raw[k] = 0u;
        }
    }

    if (path != NULL) {
        int64_t n = owx_file_read(path, s_file, (uint32_t)sizeof(s_file));
        if (n < 0) {
            owx_puts("owinstaller: cannot read '");
            owx_puts(path);
            owx_puts("' (gateway code ");
            owx_put_i64(n);
            owx_puts(")\r\n");
            return 4;
        }
        owinstaller_parse(s_file, (uint32_t)n, &m);
    } else {
        owinstaller_parse(owinstaller_default_manifest,
                          (uint32_t)(sizeof(owinstaller_default_manifest) - 1u),
                          &m);
    }

    if (m.name[0] == '\0') {
        owinstaller_copy(m.name, OWINSTALLER_NAME_MAX, "(unnamed)");
    }
    if (m.version[0] == '\0') {
        owinstaller_copy(m.version, OWINSTALLER_VER_MAX, "0.0.0");
    }

    owx_puts("owinstaller: OpenWindows Package Installer (model)\r\n");
    owx_puts("owinstaller: package=");
    owx_puts(m.name);
    owx_puts(" version=");
    owx_puts(m.version);
    owx_puts(" source=");
    owx_puts(path != NULL ? path : "built-in");
    owx_puts("\r\n");

    owx_puts("owinstaller: dependency resolution:\r\n");
    for (i = 0u; i < m.dep_count; i++) {
        const char *installed = NULL;
        uint32_t j;
        for (j = 0u; j < OWINSTALLER_INSTALLED_COUNT; j++) {
            if (owinstaller_streq(owinstaller_installed[j].name, m.deps[i].name)) {
                installed = owinstaller_installed[j].version;
                break;
            }
        }
        owx_puts("  ");
        owx_puts(m.deps[i].name);
        owx_puts(" >= ");
        owx_puts(m.deps[i].min[0] != '\0' ? m.deps[i].min : "any");
        if (installed == NULL) {
            owx_puts("  MISSING\r\n");
        } else if (m.deps[i].min[0] != '\0' &&
                   owinstaller_vercmp(installed, m.deps[i].min) < 0) {
            owx_puts("  TOO OLD (installed ");
            owx_puts(installed);
            owx_puts(")\r\n");
        } else {
            owx_puts("  satisfied (installed ");
            owx_puts(installed);
            owx_puts(")\r\n");
            satisfied++;
        }
    }
    if (m.dep_count == 0u) {
        owx_puts("  (no dependencies)\r\n");
    }

    for (i = 0u; i < m.file_count; i++) {
        total_bytes += m.files[i].size;
    }

    owx_puts("owinstaller: install plan (");
    owx_put_u64((uint64_t)m.file_count);
    owx_puts(" files, ");
    owx_put_u64((uint64_t)total_bytes);
    owx_puts(" bytes):\r\n");
    for (i = 0u; i < m.file_count; i++) {
        owx_puts("  [");
        owx_put_u64((uint64_t)(i + 1u));
        owx_puts("/");
        owx_put_u64((uint64_t)m.file_count);
        owx_puts("] ");
        owx_puts(m.files[i].path);
        owx_puts(" ");
        owx_put_u64((uint64_t)m.files[i].size);
        owx_puts(" bytes sum=");
        owinstaller_put_hex8(m.files[i].sum);
        owx_puts("\r\n");
    }

    if (m.dep_count != 0u && satisfied < m.dep_count && !force) {
        owx_puts("owinstaller: aborted: ");
        owx_put_u64((uint64_t)(m.dep_count - satisfied));
        owx_puts(" unsatisfied dependency(ies); use --force to override\r\n");
        return 4;
    }

    if (interactive) {
        owx_puts("owinstaller: proceed with installation? [y/N] ");
        {
            int c = owx_getc_wait();
            owx_puts("\r\n");
            if (c != 'y' && c != 'Y') {
                owx_puts("owinstaller: cancelled by user\r\n");
                return 0;
            }
        }
    }

    if (dry_run) {
        owx_puts("owinstaller: dry run - plan only, nothing written\r\n");
        return 0;
    }

    owx_puts("owinstaller: progress:\r\n");
    for (i = 0u; i < m.file_count; i++) {
        uint32_t pct;
        done_bytes += m.files[i].size;
        pct = (total_bytes == 0u) ? 100u : (done_bytes * 100u) / total_bytes;
        owx_puts("  ");
        owx_puts(m.files[i].path);
        owx_puts("\r\n");
        owinstaller_progress(pct);
    }
    if (m.file_count == 0u) {
        owinstaller_progress(100u);
    }

    owx_puts("owinstaller: result: installed ");
    owx_put_u64((uint64_t)m.file_count);
    owx_puts(" files, ");
    owx_put_u64((uint64_t)total_bytes);
    owx_puts(" bytes; deps ");
    owx_put_u64((uint64_t)satisfied);
    owx_putc('/');
    owx_put_u64((uint64_t)m.dep_count);
    owx_puts(" satisfied\r\n");
    owx_puts("owinstaller: status: SUCCESS\r\n");
    return 0;
}

static void owinstaller_usage(void)
{
    owx_puts(
        "usage: owinstaller [-f manifest] [-n] [-i] [--force] [-h]\r\n"
        "  -f, --file P    read a package manifest (default: built-in model)\r\n"
        "  -n, --dry-run   resolve and plan, but do not install\r\n"
        "  -i, --interactive  ask for confirmation before installing\r\n"
        "  --force         install even if a dependency is unsatisfied\r\n"
        "  -h, --help      show this help\r\n"
        "  manifest lines: name V | version V | dep NAME VER | file PATH SIZE HEXSUM\r\n");
}
