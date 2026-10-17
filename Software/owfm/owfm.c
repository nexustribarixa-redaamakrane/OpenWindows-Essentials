/*
 * owfm.c - OpenWindows File Manager (.owx)
 *
 * A two-pane file manager rendered as text.  The volume tree is a fixed,
 * zero-allocation model (there is no readdir syscall from Ring 3); the pane
 * state machine - current directory, the listing view, and the selected row -
 * is real, and every command the interactive session understands can also be
 * scripted as arguments.  Opening a file exercises the OWFS read gateway.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* ── Constants ────────────────────────────────────────────────── */

#define OWFM_VERSION "1.0.0"

/* One entry of the modelled volume.  `parent` indexes this table; -1 is the
 * volume root. */
typedef struct {
    const char *name;
    int32_t     parent;
    bool        is_dir;
    uint32_t    size;
} owfm_node_t;

static const owfm_node_t owfm_nodes[] = {
    { "\\",         -1, true,  0u       },
    { "Documents",   0, true,  0u       },
    { "Programs",    0, true,  0u       },
    { "Media",       0, true,  0u       },
    { "readme.txt",  0, false, 1024u    },
    { "system.kconf",0, false, 512u     },
    { "guide.txt",   1, false, 4096u    },
    { "notes.txt",   1, false, 2048u    },
    { "archive",     1, true,  0u       },
    { "old1.log",    8, false, 300u     },
    { "old2.log",    8, false, 280u     },
    { "owsh.owx",    2, false, 16384u   },
    { "owls.owx",    2, false, 12288u   },
    { "owcat.owx",   2, false, 8192u    },
    { "song1.wav",   3, false, 1048576u },
    { "song2.wav",   3, false, 2097152u },
    { "pic.suf",     3, false, 65536u   }
};

#define OWFM_NODE_COUNT (sizeof(owfm_nodes) / sizeof(owfm_nodes[0]))

#define OWFM_PANE_ROWS  8u
#define OWFM_LEFT_W     26u
#define OWFM_RIGHT_W    34u
#define OWFM_PATH_MAX   128u
#define OWFM_LINE_MAX   96u
#define OWFM_FILEBUF_MAX 256u

static int32_t  s_cwd;
static uint32_t s_cursor;
static int32_t  s_view[OWFM_NODE_COUNT];
static uint32_t s_view_len;
static char     s_path[OWFM_PATH_MAX];
static char     s_filebuf[OWFM_FILEBUF_MAX];

/* ── Forward declarations ─────────────────────────────────────── */

static int  owfm_run(int argc, const char *const *argv);
static void owfm_usage(void);

/* ── Entry point ──────────────────────────────────────────────── */

int owx_main(int argc, const char *const *argv)
{
    if (argc < 1) {
        owfm_usage();
        return 1;
    }
    return owfm_run(argc, argv);
}

/* ── Helpers ──────────────────────────────────────────────────── */

static bool owfm_streq(const char *a, const char *b)
{
    while (*a != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == *b;
}

static bool owfm_parse_u32(const char *s, uint32_t *out)
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
        if (v > 1000000u) {
            return false;
        }
        s++;
    }
    *out = (uint32_t)v;
    return true;
}

/* Append `src` to `dst` without overrunning `cap` (NUL included). */
static void owfm_append(char *dst, uint32_t cap, const char *src)
{
    uint32_t pos = 0u;
    while (dst[pos] != '\0') {
        pos++;
    }
    while (*src != '\0' && pos + 1u < cap) {
        dst[pos++] = *src++;
    }
    dst[pos] = '\0';
}

/* Build the printable path of a node into s_path. */
static void owfm_build_path(int32_t node)
{
    int32_t stack[OWFM_NODE_COUNT];
    uint32_t depth = 0u;
    int32_t cur = node;

    while (cur >= 0 && depth < OWFM_NODE_COUNT) {
        stack[depth++] = cur;
        cur = owfm_nodes[cur].parent;
    }

    s_path[0] = '\0';
    if (depth == 0u) {
        owfm_append(s_path, OWFM_PATH_MAX, "/");
        return;
    }
    /* stack[depth-1] is the root; emit the components below it, leaf last. */
    owfm_append(s_path, OWFM_PATH_MAX, "/");
    {
        uint32_t k = depth - 1u;
        while (k > 0u) {
            k--;
            owfm_append(s_path, OWFM_PATH_MAX, owfm_nodes[stack[k]].name);
            owfm_append(s_path, OWFM_PATH_MAX, "/");
        }
    }
}

/* Copy `text` into `buf`, truncated or space-padded to exactly `width`. */
static void owfm_fill(char *buf, uint32_t width, const char *text)
{
    uint32_t i = 0u;
    while (i < width && text[i] != '\0') {
        buf[i] = text[i];
        i++;
    }
    while (i < width) {
        buf[i++] = ' ';
    }
    buf[width] = '\0';
}

static void owfm_dec(uint32_t v, char *out)
{
    char tmp[11];
    uint32_t n = 0u;
    uint32_t i;
    if (v == 0u) {
        out[0] = '0';
        out[1] = '\0';
        return;
    }
    while (v > 0u && n < sizeof(tmp)) {
        tmp[n++] = (char)('0' + (int)(v % 10u));
        v /= 10u;
    }
    for (i = 0u; i < n; i++) {
        out[i] = tmp[n - 1u - i];
    }
    out[n] = '\0';
}

/* Recompute the listing of the current directory and clamp the cursor. */
static void owfm_refresh(void)
{
    uint32_t i;
    s_view_len = 0u;
    for (i = 0u; i < OWFM_NODE_COUNT; i++) {
        if (owfm_nodes[i].parent == s_cwd) {
            s_view[s_view_len++] = (int32_t)i;
        }
    }
    if (s_view_len == 0u) {
        s_cursor = 0u;
    } else if (s_cursor >= s_view_len) {
        s_cursor = s_view_len - 1u;
    }
}

static void owfm_right_line(uint32_t row, char *buf)
{
    char num[12];
    buf[0] = '\0';
    if (s_view_len == 0u) {
        if (row == 0u) {
            owfm_append(buf, OWFM_RIGHT_W + 1u, " (empty directory)");
        }
        return;
    }
    {
        int32_t node = s_view[s_cursor];
        const owfm_node_t *e = &owfm_nodes[node];
        switch (row) {
        case 0u:
            owfm_append(buf, OWFM_RIGHT_W + 1u, " name  : ");
            owfm_append(buf, OWFM_RIGHT_W + 1u, e->name);
            break;
        case 1u:
            owfm_append(buf, OWFM_RIGHT_W + 1u, " type  : ");
            owfm_append(buf, OWFM_RIGHT_W + 1u, e->is_dir ? "directory" : "file");
            break;
        case 2u:
            owfm_append(buf, OWFM_RIGHT_W + 1u, " size  : ");
            if (e->is_dir) {
                owfm_append(buf, OWFM_RIGHT_W + 1u, "-");
            } else {
                owfm_dec(e->size, num);
                owfm_append(buf, OWFM_RIGHT_W + 1u, num);
                owfm_append(buf, OWFM_RIGHT_W + 1u, " bytes");
            }
            break;
        case 3u:
            owfm_append(buf, OWFM_RIGHT_W + 1u, " path  : ");
            owfm_append(buf, OWFM_RIGHT_W + 1u, s_path);
            break;
        case 4u:
            owfm_append(buf, OWFM_RIGHT_W + 1u, " cursor: ");
            owfm_dec(s_cursor + 1u, num);
            owfm_append(buf, OWFM_RIGHT_W + 1u, num);
            owfm_append(buf, OWFM_RIGHT_W + 1u, " of ");
            owfm_dec(s_view_len, num);
            owfm_append(buf, OWFM_RIGHT_W + 1u, num);
            break;
        case 5u:
            owfm_append(buf, OWFM_RIGHT_W + 1u, " hint  : ");
            owfm_append(buf, OWFM_RIGHT_W + 1u,
                        e->is_dir ? "cd / sel / up / ls" : "open / sel / up / ls");
            break;
        default:
            break;
        }
    }
}

static void owfm_render(void)
{
    char left[OWFM_LEFT_W + 1u];
    char right[OWFM_RIGHT_W + 1u];
    uint32_t top;
    uint32_t r;

    owfm_refresh();
    owfm_build_path(s_cwd);

    if (s_cursor >= OWFM_PANE_ROWS / 2u) {
        top = s_cursor - OWFM_PANE_ROWS / 2u;
    } else {
        top = 0u;
    }
    if (top + OWFM_PANE_ROWS > s_view_len && s_view_len > OWFM_PANE_ROWS) {
        top = s_view_len - OWFM_PANE_ROWS;
    }
    if (s_view_len <= OWFM_PANE_ROWS) {
        top = 0u;
    }

    owx_puts("owfm: OpenWindows File Manager  cwd=");
    owx_puts(s_path);
    owx_puts("  entries=");
    owx_put_u64((uint64_t)s_view_len);
    owx_puts("\r\n");
    owx_puts("+--------------------------+----------------------------------+\r\n");

    for (r = 0u; r < OWFM_PANE_ROWS; r++) {
        uint32_t idx = top + r;
        left[0] = '\0';
        if (idx < s_view_len) {
            const owfm_node_t *e = &owfm_nodes[s_view[idx]];
            owfm_append(left, sizeof(left), (idx == s_cursor) ? "> " : "  ");
            owfm_append(left, sizeof(left), e->name);
            if (e->is_dir) {
                owfm_append(left, sizeof(left), "/");
            }
        }
        owfm_right_line(r, right);
        owfm_fill(right, OWFM_RIGHT_W, right);
        {
            char lpad[OWFM_LEFT_W + 1u];
            owfm_fill(lpad, OWFM_LEFT_W, left);
            owx_puts("|");
            owx_puts(lpad);
            owx_puts("|");
            owx_puts(right);
            owx_puts("|\r\n");
        }
    }

    owx_puts("+--------------------------+----------------------------------+\r\n");
}

/* Try to read the selected file through the gateway; report the outcome. */
static void owfm_open(void)
{
    int32_t node;
    const owfm_node_t *e;
    int64_t n;

    if (s_view_len == 0u) {
        owx_puts("owfm: nothing selected\r\n");
        return;
    }
    node = s_view[s_cursor];
    e = &owfm_nodes[node];
    if (e->is_dir) {
        s_cwd = node;
        s_cursor = 0u;
        owfm_refresh();
        owx_puts("owfm: entered directory ");
        owfm_build_path(s_cwd);
        owx_puts(s_path);
        owx_puts("\r\n");
        return;
    }
    n = owx_file_read(e->name, s_filebuf, (uint32_t)sizeof(s_filebuf));
    if (n >= 0) {
        owx_puts("owfm: open ");
        owx_puts(e->name);
        owx_puts(" -> ");
        owx_put_i64(n);
        owx_puts(" bytes from OWFS\r\n");
    } else {
        owx_puts("owfm: open ");
        owx_puts(e->name);
        owx_puts(" -> gateway returned ");
        owx_put_i64(n);
        owx_puts(" (");
        if (n == OWX_GATE_VFS_ENOENT) {
            owx_puts("ENOENT");
        } else if (n == OWX_GATE_VFS_EACCES) {
            owx_puts("EACCES");
        } else if (n == OWX_GATE_VFS_EIO) {
            owx_puts("EIO");
        } else if (n == OWX_GATE_VFS_EFAULT) {
            owx_puts("EFAULT");
        } else if (n == OWX_GATE_VFS_EINVAL) {
            owx_puts("EINVAL");
        } else {
            owx_puts("error");
        }
        owx_puts(")\r\n");
    }
}

/* Execute one command.  Returns false for an unknown command. */
static bool owfm_exec(const char *cmd, const char *arg)
{
    if (owfm_streq(cmd, "ls")) {
        owfm_refresh();
        return true;
    }
    if (owfm_streq(cmd, "cd")) {
        uint32_t i;
        if (arg == NULL) {
            owx_puts("owfm: cd requires a directory name\r\n");
            return true;
        }
        for (i = 0u; i < s_view_len; i++) {
            const owfm_node_t *e = &owfm_nodes[s_view[i]];
            if (owfm_nodes[s_view[i]].parent == s_cwd && e->is_dir &&
                owfm_streq(e->name, arg)) {
                s_cwd = s_view[i];
                s_cursor = 0u;
                owfm_refresh();
                return true;
            }
        }
        owx_puts("owfm: no such directory: ");
        owx_puts(arg);
        owx_puts("\r\n");
        return true;
    }
    if (owfm_streq(cmd, "up")) {
        if (owfm_nodes[s_cwd].parent >= 0) {
            s_cwd = owfm_nodes[s_cwd].parent;
            s_cursor = 0u;
            owfm_refresh();
        }
        return true;
    }
    if (owfm_streq(cmd, "sel")) {
        uint32_t n = 0u;
        if (arg == NULL || !owfm_parse_u32(arg, &n) || n == 0u) {
            owx_puts("owfm: sel requires a 1-based index\r\n");
            return true;
        }
        if (n > s_view_len) {
            owx_puts("owfm: index out of range\r\n");
            return true;
        }
        s_cursor = n - 1u;
        return true;
    }
    if (owfm_streq(cmd, "next")) {
        if (s_view_len > 0u && s_cursor + 1u < s_view_len) {
            s_cursor++;
        }
        return true;
    }
    if (owfm_streq(cmd, "prev")) {
        if (s_cursor > 0u) {
            s_cursor--;
        }
        return true;
    }
    if (owfm_streq(cmd, "open")) {
        owfm_open();
        return true;
    }
    return false;
}

/* Read one console line with minimal editing. */
static void owfm_readline(char *buf, uint32_t cap)
{
    uint32_t n = 0u;
    for (;;) {
        int c = owx_getc_wait();
        if (c == '\r' || c == '\n' || c == 4 || c == -1) {
            owx_puts("\r\n");
            break;
        }
        if (c == 8 || c == 127) {
            if (n > 0u) {
                n--;
                owx_puts("\b \b");
            }
            continue;
        }
        if (c < 32) {
            continue;
        }
        if (n + 1u < cap) {
            buf[n++] = (char)c;
            owx_putc((char)c);
        }
    }
    buf[n] = '\0';
}

/* Split a line into a command word and the remainder. */
static bool owfm_split(char *line, const char **cmd, const char **arg)
{
    char *p = line;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\0') {
        return false;
    }
    *cmd = p;
    while (*p != '\0' && *p != ' ' && *p != '\t') {
        p++;
    }
    if (*p != '\0') {
        *p = '\0';
        p++;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        *arg = (*p != '\0') ? p : NULL;
    } else {
        *arg = NULL;
    }
    return true;
}

/* ── Implementation ───────────────────────────────────────────── */

static int owfm_run(int argc, const char *const *argv)
{
    bool bad = false;
    int i;

    s_cwd = 0;
    s_cursor = 0u;
    owfm_refresh();
    owfm_render();

    if (argc <= 1) {
        char line[OWFM_LINE_MAX];
        const char *cmd;
        const char *arg;
        owx_puts("owfm: interactive - commands: ls cd <dir> up sel <n> next prev open q\r\n");
        for (;;) {
            owx_puts("owfm> ");
            owfm_readline(line, (uint32_t)sizeof(line));
            if (!owfm_split(line, &cmd, &arg)) {
                continue;
            }
            if (owfm_streq(cmd, "q") || owfm_streq(cmd, "quit") ||
                owfm_streq(cmd, "exit")) {
                break;
            }
            if (!owfm_exec(cmd, arg)) {
                owx_puts("owfm: unknown command: ");
                owx_puts(cmd);
                owx_puts("\r\n");
            }
            owfm_render();
        }
        return 0;
    }

    /* Scripted mode: every argument is a command, and cd/sel take the next. */
    for (i = 1; i < argc; i++) {
        const char *cmd = argv[i];
        const char *arg = NULL;
        if (cmd == NULL) {
            continue;
        }
        if ((owfm_streq(cmd, "cd") || owfm_streq(cmd, "sel")) && i + 1 < argc) {
            arg = argv[++i];
        }
        if (!owfm_exec(cmd, arg)) {
            owx_puts("owfm: unknown command: ");
            owx_puts(cmd);
            owx_puts("\r\n");
            bad = true;
        }
        owfm_render();
    }

    return bad ? 2 : 0;
}

static void owfm_usage(void)
{
    owx_puts(
        "usage: owfm [command ...]\r\n"
        "  ls              list the current directory\r\n"
        "  cd <dir>        enter a subdirectory\r\n"
        "  up              go to the parent directory\r\n"
        "  sel <n>         select row n (1-based)\r\n"
        "  next | prev     move the selection\r\n"
        "  open            open (descend) the selection; files are read via OWFS\r\n"
        "  With no arguments, owfm runs an interactive session.\r\n");
}
