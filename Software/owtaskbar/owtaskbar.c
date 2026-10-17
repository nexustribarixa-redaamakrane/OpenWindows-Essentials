/*
 * owtaskbar.c - OpenWindows Taskbar Application (.owx)
 *
 * A headless model of the desktop taskbar.  It maintains a task list (add,
 * remove, activate, deactivate) and renders the bar as a text strip with the
 * active task highlighted, then reports the list state.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWTASKBAR_VERSION "1.0.0"
#define OWTASKBAR_MAX_TASKS 12u
#define OWTASKBAR_NAME_MAX  24u

typedef struct {
    char name[OWTASKBAR_NAME_MAX];
} owtaskbar_task_t;

static owtaskbar_task_t s_tasks[OWTASKBAR_MAX_TASKS];
static uint32_t s_count;
static int32_t  s_active;    /* index, or -1 for none */

/* ── Forward declarations ─────────────────────────────────────── */

static int  owtaskbar_run(int argc, const char *const *argv);
static void owtaskbar_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owtaskbar_usage();
        return 1;
    }
    return owtaskbar_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owtaskbar_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static void owtaskbar_copy(char *dst, uint32_t cap, const char *src)
{
    uint32_t i = 0u;
    while (i + 1u < cap && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static bool owtaskbar_parse_u32(const char *s, uint32_t *out)
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
        if (v > 1000u) {
            return false;
        }
        s++;
    }
    *out = (uint32_t)v;
    return true;
}

static void owtaskbar_render(void)
{
    uint32_t i;
    owx_puts("owtaskbar: [ Start ] ");
    for (i = 0u; i < s_count; i++) {
        bool act = ((int32_t)i == s_active);
        if (act) {
            owx_putc('[');
        } else {
            owx_putc(' ');
        }
        owx_puts(s_tasks[i].name);
        owx_puts(act ? "*" : " ");
        if (act) {
            owx_putc(']');
        } else {
            owx_putc(' ');
        }
        owx_puts(" ");
    }
    if (s_count == 0u) {
        owx_puts("(no tasks)");
    }
    owx_puts("\r\n");
}

static void owtaskbar_report(void)
{
    uint32_t i;
    owx_puts("owtaskbar: ");
    owx_put_u64((uint64_t)s_count);
    owx_puts(" task(s), active=");
    if (s_active >= 0) {
        owx_puts(s_tasks[s_active].name);
    } else {
        owx_puts("none");
    }
    owx_puts("\r\n");
    for (i = 0u; i < s_count; i++) {
        owx_puts("  ");
        owx_put_u64((uint64_t)(i + 1u));
        owx_puts(". ");
        owx_puts(s_tasks[i].name);
        if ((int32_t)i == s_active) {
            owx_puts("  <active>");
        }
        owx_puts("\r\n");
    }
}

static void owtaskbar_add(const char *name)
{
    if (name == NULL || name[0] == '\0') {
        owx_puts("owtaskbar: add requires a name\r\n");
        return;
    }
    if (s_count >= OWTASKBAR_MAX_TASKS) {
        owx_puts("owtaskbar: task list is full\r\n");
        return;
    }
    owtaskbar_copy(s_tasks[s_count].name, OWTASKBAR_NAME_MAX, name);
    s_count++;
    if (s_active < 0) {
        s_active = (int32_t)(s_count - 1u);
    }
    owx_puts("owtaskbar: added ");
    owx_puts(name);
    owx_puts("\r\n");
}

static void owtaskbar_remove(uint32_t n)
{
    uint32_t k;
    if (n == 0u || n > s_count) {
        owx_puts("owtaskbar: no such task\r\n");
        return;
    }
    for (k = n - 1u; k + 1u < s_count; k++) {
        s_tasks[k] = s_tasks[k + 1u];
    }
    s_count--;
    if (s_active == (int32_t)(n - 1u)) {
        s_active = (s_count > 0u) ? 0 : -1;
    } else if (s_active > (int32_t)(n - 1u)) {
        s_active--;
    }
    owx_puts("owtaskbar: removed task ");
    owx_put_u64((uint64_t)n);
    owx_puts("\r\n");
}

static void owtaskbar_activate(uint32_t n)
{
    if (n == 0u || n > s_count) {
        owx_puts("owtaskbar: no such task\r\n");
        return;
    }
    s_active = (int32_t)(n - 1u);
    owx_puts("owtaskbar: activated ");
    owx_puts(s_tasks[s_active].name);
    owx_puts("\r\n");
}

/* ── Implementation ───────────────────────────────────────────── */

static int owtaskbar_run(int argc, const char *const *argv)
{
    int i;
    int rc = 0;

    s_count = 0u;
    s_active = -1;

    if (argc <= 1) {
        /* A default desktop session, so the bar is not empty. */
        owtaskbar_add("Documents");
        owtaskbar_add("Terminal");
        owtaskbar_add("Settings");
        owtaskbar_activate(2u);
        owtaskbar_render();
        owtaskbar_report();
        return 0;
    }

    for (i = 1; i < argc; i++) {
        const char *cmd = argv[i];
        if (cmd == NULL) {
            continue;
        }
        if (owtaskbar_streq(cmd, "add")) {
            if (i + 1 >= argc || argv[i + 1] == NULL) {
                owx_puts("owtaskbar: add requires a name\r\n");
                rc = 2;
                continue;
            }
            owtaskbar_add(argv[++i]);
        } else if (owtaskbar_streq(cmd, "remove")) {
            uint32_t n = 0u;
            if (i + 1 >= argc || !owtaskbar_parse_u32(argv[i + 1], &n)) {
                owx_puts("owtaskbar: remove requires a 1-based index\r\n");
                rc = 2;
                continue;
            }
            i++;
            owtaskbar_remove(n);
        } else if (owtaskbar_streq(cmd, "activate")) {
            uint32_t n = 0u;
            if (i + 1 >= argc || !owtaskbar_parse_u32(argv[i + 1], &n)) {
                owx_puts("owtaskbar: activate requires a 1-based index\r\n");
                rc = 2;
                continue;
            }
            i++;
            owtaskbar_activate(n);
        } else if (owtaskbar_streq(cmd, "deactivate")) {
            s_active = -1;
            owx_puts("owtaskbar: no active task\r\n");
        } else if (owtaskbar_streq(cmd, "list")) {
            owtaskbar_report();
            continue;
        } else if (owtaskbar_streq(cmd, "render")) {
            owtaskbar_render();
            continue;
        } else if (owtaskbar_streq(cmd, "-h") || owtaskbar_streq(cmd, "--help")) {
            owtaskbar_usage();
            return 0;
        } else {
            owx_puts("owtaskbar: unknown command '");
            owx_puts(cmd);
            owx_puts("'\r\n");
            owtaskbar_usage();
            return 2;
        }

        owtaskbar_render();
        owtaskbar_report();
    }

    return rc;
}

static void owtaskbar_usage(void)
{
    owx_puts(
        "usage: owtaskbar [command ...]\r\n"
        "  add <name>       append a task\r\n"
        "  remove <n>       delete task n (1-based)\r\n"
        "  activate <n>     make task n the active task\r\n"
        "  deactivate       clear the active task\r\n"
        "  list             print the task list and the active task\r\n"
        "  render           draw the taskbar strip\r\n"
        "  With no arguments a default desktop session is shown.\r\n");
}
