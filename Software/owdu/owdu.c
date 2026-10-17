/*
 * owdu.c - OpenWindows Disk Usage Utility (.owx)
 *
 * Summarizes the size of OpenWindows Catalogs and Inodes.
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owdu summary [catalog]        Report total allocation for catalog
 *   owdu tree [catalog]           Display recursive breakdown of catalog tree
 *   owdu query <path>             Display allocation for a specific inode/catalog
 *   owdu help                     Display usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWDU_VERSION    "1.2.0"
#define OWDU_READ_MAX   16384u
#define OWDU_MAX_ARGS   16u

static uint8_t s_read_buf[OWDU_READ_MAX];

/* ── Catalog / Inode Model ────────────────────────────────────── */

typedef struct {
    const char *path;
    uint64_t    size;      /* bytes; catalogs carry 0 and are summed */
    bool        is_catalog;
} owdu_node_t;

static const owdu_node_t owdu_model[] = {
    { "\\",                                  0u, true  },
    { "\\System",                            0u, true  },
    { "\\System\\Kernel",                    0u, true  },
    { "\\System\\Kernel\\kernel.owx",  1048576u, false },
    { "\\System\\DLL",                       0u, true  },
    { "\\System\\DLL\\htl.owd",           65536u, false },
    { "\\System\\DLL\\bootvid.owd",       32768u, false },
    { "\\System\\DLL\\owrp.owd",          49152u, false },
    { "\\System\\Config",                    0u, true  },
    { "\\System\\Config\\system.kconf",    8192u, false },
    { "\\Users",                             0u, true  },
    { "\\Users\\Administrator",              0u, true  },
    { "\\Users\\Default",                    0u, true  },
    { "\\Users\\Default\\Documents",         0u, true  },
    { "\\Users\\Default\\Documents\\readme.txt", 4096u, false },
    { "\\Users\\Default\\pic.suf",       262144u, false },
    { "\\Temp",                              0u, true  },
    { "\\Temp\\log.txt",                  12288u, false }
};

/* ── Tiny string / number helpers ─────────────────────────────── */

static uint32_t owdu_strlen(const char *s)
{
    uint32_t n = 0u;
    if (!s) return 0u;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static bool owdu_streq(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a != '\0' && *b != '\0') {
        char ca = (*a == '/') ? '\\' : *a;
        char cb = (*b == '/') ? '\\' : *b;
        if (ca != cb) return false;
        a++;
        b++;
    }
    return *a == *b;
}

static bool owdu_prefix(const char *s, const char *prefix)
{
    if (!s || !prefix) return false;
    while (*prefix != '\0') {
        char cs = (*s == '/') ? '\\' : *s;
        char cp = (*prefix == '/') ? '\\' : *prefix;
        if (cs != cp) {
            return false;
        }
        s++;
        prefix++;
    }
    return true;
}

static bool owdu_is_under(const char *child, const char *parent)
{
    uint32_t plen;
    uint32_t clen;

    if (owdu_streq(child, parent)) {
        return false;
    }
    if ((parent[0] == '\\' || parent[0] == '/') && parent[1] == '\0') {
        return (child[0] == '\\' || child[0] == '/') && child[1] != '\0';
    }
    plen = owdu_strlen(parent);
    clen = owdu_strlen(child);
    if (clen <= plen || !owdu_prefix(child, parent)) {
        return false;
    }
    return child[plen] == '\\' || child[plen] == '/';
}

static const owdu_node_t *owdu_find(const char *path)
{
    uint32_t i;
    const uint32_t n = (uint32_t)(sizeof(owdu_model) / sizeof(owdu_model[0]));
    for (i = 0u; i < n; i++) {
        if (owdu_streq(owdu_model[i].path, path)) {
            return &owdu_model[i];
        }
    }
    return NULL;
}

static uint64_t owdu_catalog_size(const char *cat)
{
    uint32_t i;
    uint64_t total = 0u;
    const uint32_t n = (uint32_t)(sizeof(owdu_model) / sizeof(owdu_model[0]));
    for (i = 0u; i < n; i++) {
        if (owdu_is_under(owdu_model[i].path, cat)) {
            total += owdu_model[i].size;
        }
    }
    return total;
}

static void owdu_fmt_size(uint64_t bytes, char *out, uint32_t cap)
{
    const char units[] = { 'B', 'K', 'M', 'G', 'T' };
    uint32_t u = 0u;
    uint64_t val = bytes;
    char tmp[32];
    uint32_t tn = 0u;
    uint32_t on = 0u;

    if (cap == 0u) return;

    while (val >= 1024u && u + 1u < sizeof(units)) {
        val /= 1024u;
        u++;
    }

    if (val == 0u) {
        tmp[tn++] = '0';
    } else {
        while (val > 0u && tn < 20u) {
            tmp[tn++] = (char)('0' + (val % 10u));
            val /= 10u;
        }
    }

    while (tn > 0u && on + 2u < cap) {
        out[on++] = tmp[--tn];
    }
    if (u > 0u && on + 1u < cap) {
        out[on++] = ' ';
        out[on++] = units[u];
        if (units[u] != 'B' && on + 1u < cap) {
            out[on++] = 'i';
            out[on++] = 'B';
        }
    } else if (on + 1u < cap) {
        out[on++] = ' ';
        out[on++] = 'B';
    }
    out[on] = '\0';
}

static void owdu_usage(void)
{
    owx_puts("owdu: OpenWindows Catalog Disk Usage Utility v" OWDU_VERSION "\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owdu summary [catalog]      summarize catalog usage\r\n");
    owx_puts("  owdu tree [catalog]         list recursive tree breakdown\r\n");
    owx_puts("  owdu query <path>           display specific inode size\r\n");
    owx_puts("  owdu help                   display this information\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb = "tree";
    const char *target = "\\";
    char size_str[32];

    if (argc >= 2) {
        if (owdu_streq(argv[1], "help") || owdu_streq(argv[1], "-h") || owdu_streq(argv[1], "--help")) {
            owdu_usage();
            return 0;
        }
        if (owdu_streq(argv[1], "summary") || owdu_streq(argv[1], "-s")) {
            verb = "summary";
            if (argc >= 3) target = argv[2];
        } else if (owdu_streq(argv[1], "tree")) {
            verb = "tree";
            if (argc >= 3) target = argv[2];
        } else if (owdu_streq(argv[1], "query")) {
            verb = "query";
            if (argc >= 3) target = argv[2];
            else {
                owx_puts("owdu: query requires a path argument\r\n");
                return 1;
            }
        } else if (argv[1][0] != '-') {
            target = argv[1];
        }
    }

    if (owdu_streq(verb, "query")) {
        const owdu_node_t *node = owdu_find(target);
        uint64_t sz = 0u;
        if (node != NULL) {
            sz = node->is_catalog ? owdu_catalog_size(target) : node->size;
        } else {
            int64_t n = owx_file_read(target, s_read_buf, (uint32_t)sizeof(s_read_buf));
            if (n < 0) {
                owx_puts("owdu: cannot access '");
                owx_puts(target);
                owx_puts("': inode or catalog not found\r\n");
                return 1;
            }
            sz = (uint64_t)n;
        }
        owdu_fmt_size(sz, size_str, sizeof(size_str));
        owx_puts(target);
        owx_puts("\t");
        owx_puts(size_str);
        owx_puts("\r\n");
        return 0;
    }

    if (owdu_streq(verb, "summary")) {
        uint64_t sz = owdu_catalog_size(target);
        owdu_fmt_size(sz, size_str, sizeof(size_str));
        owx_puts("Catalog: ");
        owx_puts(target);
        owx_puts(" total usage: ");
        owx_puts(size_str);
        owx_puts("\r\n");
        return 0;
    }

    /* Verb: tree */
    owx_puts("ALLOCATION   TYPE     CATALOG / INODE\r\n");
    owx_puts("-----------  -------  ------------------------------------\r\n");
    for (uint32_t i = 0u; i < (uint32_t)(sizeof(owdu_model) / sizeof(owdu_model[0])); i++) {
        const owdu_node_t *d = &owdu_model[i];
        if (owdu_streq(d->path, target) || owdu_is_under(d->path, target)) {
            uint64_t sz = d->is_catalog ? owdu_catalog_size(d->path) : d->size;
            owdu_fmt_size(sz, size_str, sizeof(size_str));
            uint32_t slen = owdu_strlen(size_str);
            owx_puts(size_str);
            for (uint32_t p = slen; p < 13u; p++) owx_putc(' ');
            owx_puts(d->is_catalog ? "CATALOG  " : "INODE    ");
            owx_puts(d->path);
            owx_puts("\r\n");
        }
    }
    return 0;
}
