/*
 * owumount.c - OpenWindows Volume Detach Utility (.owx)
 *
 * Detaches a mounted storage volume or VIP index from its catalog binding.
 *
 * Conforms to OpenWindows non-POSIX verb-driven CLI conventions:
 *   owumount detach <volume|catalog> [force]   Detach volume from catalog
 *   owumount list                             List attached volumes
 *   owumount help                             Show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWUMOUNT_VERSION "1.2.0"

#define OWUMOUNT_MAX_MOUNTS 8u
#define OWUMOUNT_DEV_MAX    48u
#define OWUMOUNT_PATH_MAX   48u
#define OWUMOUNT_FS_MAX     16u

/* ── Mount namespace ──────────────────────────────────────────── */

typedef struct {
    char device[OWUMOUNT_DEV_MAX];
    char mountpoint[OWUMOUNT_PATH_MAX];
    char fstype[OWUMOUNT_FS_MAX];
    bool read_only;
    bool busy;
    bool system_root;
    bool used;
} owumount_ent_t;

static owumount_ent_t s_mounts[OWUMOUNT_MAX_MOUNTS];

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owumount_streq(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a != '\0' && *b != '\0') {
        char ca = (*a == '/') ? '\\' : *a;
        char cb = (*b == '/') ? '\\' : *b;
        if (ca != cb) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static bool owumount_copy(char *dst, const char *src, uint32_t cap)
{
    uint32_t i = 0u;
    if (cap == 0u || !dst || !src) {
        return false;
    }
    while (src[i] != '\0') {
        if (i + 1u >= cap) {
            dst[0] = '\0';
            return false;
        }
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return true;
}

static void owumount_seed(void)
{
    uint32_t i;
    for (i = 0u; i < OWUMOUNT_MAX_MOUNTS; i++) {
        s_mounts[i].used = false;
        s_mounts[i].busy = false;
        s_mounts[i].read_only = false;
        s_mounts[i].system_root = false;
        s_mounts[i].device[0] = '\0';
        s_mounts[i].mountpoint[0] = '\0';
        s_mounts[i].fstype[0] = '\0';
    }
    (void)owumount_copy(s_mounts[0].device, "\\Device\\Harddisk0\\Partition1", OWUMOUNT_DEV_MAX);
    (void)owumount_copy(s_mounts[0].mountpoint, "\\", OWUMOUNT_PATH_MAX);
    (void)owumount_copy(s_mounts[0].fstype, "OWFS v1.2", OWUMOUNT_FS_MAX);
    s_mounts[0].used = true;
    s_mounts[0].busy = true;
    s_mounts[0].system_root = true;

    (void)owumount_copy(s_mounts[1].device, "\\Device\\Harddisk1\\Partition1", OWUMOUNT_DEV_MAX);
    (void)owumount_copy(s_mounts[1].mountpoint, "\\Storage", OWUMOUNT_PATH_MAX);
    (void)owumount_copy(s_mounts[1].fstype, "USFS v1.2", OWUMOUNT_FS_MAX);
    s_mounts[1].used = true;
    s_mounts[1].busy = false;
    s_mounts[1].system_root = false;
}

static owumount_ent_t *owumount_lookup(const char *target)
{
    uint32_t i;
    for (i = 0u; i < OWUMOUNT_MAX_MOUNTS; i++) {
        if (!s_mounts[i].used) {
            continue;
        }
        if (owumount_streq(s_mounts[i].mountpoint, target) ||
            owumount_streq(s_mounts[i].device, target)) {
            return &s_mounts[i];
        }
    }
    return NULL;
}

static void owumount_list(void)
{
    uint32_t i;
    owx_puts("VOLUME DEVICE                 CATALOG BINDING   TYPE       FLAGS\r\n");
    owx_puts("----------------------------  ----------------  ---------  ------------------\r\n");
    for (i = 0u; i < OWUMOUNT_MAX_MOUNTS; i++) {
        if (!s_mounts[i].used) {
            continue;
        }
        owx_puts(s_mounts[i].device);
        for (uint32_t p = 0; p < (s_mounts[i].device[0] ? 30 - 28 : 2); p++) owx_putc(' ');
        owx_puts("  ");
        owx_puts(s_mounts[i].mountpoint);
        owx_puts("  ");
        owx_puts(s_mounts[i].fstype);
        owx_puts(s_mounts[i].read_only ? "  READONLY" : "  READWRITE");
        if (s_mounts[i].system_root) {
            owx_puts(" [SYSTEM_ROOT]");
        } else if (s_mounts[i].busy) {
            owx_puts(" [BUSY]");
        }
        owx_puts("\r\n");
    }
}

static void owumount_usage(void)
{
    owx_puts("owumount: OpenWindows Volume Detach Utility v" OWUMOUNT_VERSION "\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owumount detach <volume|catalog> [force]   detach volume binding\r\n");
    owx_puts("  owumount list                             list active volume bindings\r\n");
    owx_puts("  owumount help                             show this information\r\n");
}

int owx_main(int argc, const char *const *argv)
{
    const char *verb = "list";
    const char *target = NULL;
    bool force = false;

    owumount_seed();

    if (argc < 2) {
        owumount_list();
        return 0;
    }

    if (owumount_streq(argv[1], "help") || owumount_streq(argv[1], "-h") || owumount_streq(argv[1], "--help")) {
        owumount_usage();
        return 0;
    }

    if (owumount_streq(argv[1], "list") || owumount_streq(argv[1], "-l")) {
        owumount_list();
        return 0;
    }

    if (owumount_streq(argv[1], "detach")) {
        verb = "detach";
        if (argc >= 3) target = argv[2];
        if (argc >= 4 && owumount_streq(argv[3], "force")) force = true;
    } else {
        target = argv[1];
        if (argc >= 3 && (owumount_streq(argv[2], "-f") || owumount_streq(argv[2], "force"))) {
            force = true;
        }
    }

    (void)verb;
    if (target == NULL) {
        owumount_usage();
        return 1;
    }

    owumount_ent_t *e = owumount_lookup(target);
    if (e == NULL) {
        owx_puts("owumount: target not bound: ");
        owx_puts(target);
        owx_puts("\r\n");
        return 1;
    }

    if (e->system_root && !force) {
        owx_puts("owumount: refusing to detach system root volume\r\n");
        return 1;
    }

    if (e->busy && !force) {
        owx_puts("owumount: volume is busy; specify force to detach\r\n");
        return 1;
    }

    e->used = false;
    owx_puts("owumount: detached volume ");
    owx_puts(e->device);
    owx_puts(" from ");
    owx_puts(e->mountpoint);
    if (force) owx_puts(" (forced)");
    owx_puts("\r\n");
    return 0;
}
