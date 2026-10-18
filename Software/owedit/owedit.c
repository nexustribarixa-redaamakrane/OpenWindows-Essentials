/*
 * owedit.c - OpenWindows text editor (.owx)
 *
 * A line-oriented document editor over the Ring 3 gateway: documents are read
 * out of the primary OWFS volume with OWX_GATE_VFS_READ, edited in a static
 * aligned buffer, and rendered back through the UART console.
 *
 * The gateway a Ring 3 image sees exposes VFS_READ but no VFS_WRITE (see
 * OpenWindows/inc/ow_syscall.h), so owedit is honest about persistence: it
 * edits in memory and can print the result, but it cannot write the document
 * back to the volume.  `owedit edit` says so on Ctrl+S and emits the edited
 * document on the console instead, so the operator can capture it.
 *
 *   owedit                  run the buffer selftest
 *   owedit open <path>      load a document and print it with a byte count
 *   owedit edit [path]      edit interactively (Ctrl+L redraw, Ctrl+S emit,
 *                           Ctrl+Q quit; printable keys insert, Backspace
 *                           deletes, Enter inserts a newline)
 *   owedit selftest         run the buffer selftest
 *   owedit help             show usage information
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../owx_gate.h"

/* One page per document: enough for the small text files the shell creates,
 * and it keeps every load to a single gateway call. */
#define OWEDIT_DOC_CAP 4096U

/* The document lives at file scope in a union whose first member is a
 * uint64_t, so its alignment matches any overlay the loader's zeroed BSS
 * gives it, without a heap or a compiler-specific aligned attribute. */
static union {
    uint64_t align;
    uint8_t  bytes[OWEDIT_DOC_CAP];
} g_DocumentImage;

typedef struct OwEditDocument {
    uint8_t *data;
    uint32_t size;   /* valid bytes in data */
    uint32_t cursor; /* insert position, 0..size */
    bool     dirty;
} OwEditDocument;

static const uint8_t OWEDIT_KEY_L = 0x0CU; /* Ctrl+L: redraw */
static const uint8_t OWEDIT_KEY_S = 0x13U; /* Ctrl+S: emit/save */
static const uint8_t OWEDIT_KEY_Q = 0x11U; /* Ctrl+Q: quit    */

/* ---------------------------------------------------------------- */
/* Helpers                                                          */
/* ---------------------------------------------------------------- */

static bool OweditStrEqual(const char *a, const char *b)
{
    if (!a || !b) return false;
    while (*a && *b) {
        if (*a != *b) return false;
        a++;
        b++;
    }
    return (*a == *b);
}

static uint32_t OweditStrLen(const char *s)
{
    uint32_t n = 0;
    if (!s) return 0;
    while (s[n] != '\0') n++;
    return n;
}

static void OweditPrintUsage(void)
{
    owx_puts("owedit: OpenWindows text editor\r\n");
    owx_puts("usage:\r\n");
    owx_puts("  owedit                  run the buffer selftest\r\n");
    owx_puts("  owedit open <path>      load a document and print it\r\n");
    owx_puts("  owedit edit [path]      edit interactively\r\n");
    owx_puts("  owedit selftest         run the buffer selftest\r\n");
    owx_puts("  owedit help             show this message\r\n");
    owx_puts("note: the ring 3 gateway has no VFS_WRITE, so edits stay in\r\n");
    owx_puts("      memory; Ctrl+S prints the document instead of saving it.\r\n");
}

static void OweditInit(OwEditDocument *doc)
{
    doc->data = g_DocumentImage.bytes;
    doc->size = 0;
    doc->cursor = 0;
    doc->dirty = false;
}

/* Raw bytes reach the console one character at a time; anything outside the
 * printable ASCII range is shown as '.', so a binary file cannot wedge the
 * serial console. */
static void OweditPrintBytes(const uint8_t *data, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; i++) {
        uint8_t c = data[i];
        if (c == (uint8_t)'\n') {
            owx_puts("\r\n");
        } else if (c >= 0x20U && c < 0x7FU) {
            owx_putc((char)c);
        } else {
            owx_putc('.');
        }
    }
}

static void OweditDraw(const OwEditDocument *doc, const char *path)
{
    owx_puts("owedit: '");
    owx_puts(path ? path : "(unsaved)");
    owx_puts(doc->dirty ? "' (modified)\r\n" : "'\r\n");
    owx_puts("-- document ");
    owx_put_u64(doc->size);
    owx_puts(" bytes, cursor ");
    owx_put_u64(doc->cursor);
    owx_puts(" --\r\n");
    OweditPrintBytes(doc->data, doc->size);
    owx_puts("\r\n-- end --\r\n");
}

static void OweditInsert(OwEditDocument *doc, uint8_t c)
{
    uint32_t i;

    if (doc->size + 1U >= (uint32_t)OWEDIT_DOC_CAP) {
        owx_puts("owedit: document is full\r\n");
        return;
    }
    for (i = doc->size; i > doc->cursor; i--) {
        doc->data[i] = doc->data[i - 1U];
    }
    doc->data[doc->cursor] = c;
    doc->size++;
    doc->cursor++;
    doc->dirty = true;
}

static void OweditBackspace(OwEditDocument *doc)
{
    uint32_t i;

    if (doc->cursor == 0U) {
        return;
    }
    for (i = doc->cursor - 1U; i + 1U < doc->size; i++) {
        doc->data[i] = doc->data[i + 1U];
    }
    doc->cursor--;
    doc->size--;
    doc->dirty = true;
}

/* ---------------------------------------------------------------- */
/* Document I/O                                                     */
/* ---------------------------------------------------------------- */

static int OweditLoad(OwEditDocument *doc, const char *path)
{
    int64_t bytes_read;

    if (!path || !*path) {
        owx_puts("owedit: missing <path>\r\n");
        return -1;
    }

    bytes_read = owx_file_read(path, doc->data, (uint32_t)OWEDIT_DOC_CAP);
    if (bytes_read < 0) {
        owx_puts("owedit: cannot read '");
        owx_puts(path);
        owx_puts("' (error code: ");
        owx_put_i64(bytes_read);
        owx_puts(")\r\n");
        return -1;
    }
    if (bytes_read > (int64_t)OWEDIT_DOC_CAP) {
        owx_puts("owedit: '");
        owx_puts(path);
        owx_puts("' exceeds the edit buffer\r\n");
        return -1;
    }

    doc->size = (uint32_t)bytes_read;
    doc->cursor = doc->size;
    doc->dirty = false;
    return 0;
}

/* The gateway has no VFS_WRITE, so "saving" is an honest refusal followed by
 * the document on the console: the operator gets the bytes, and the volume is
 * never touched. */
static void OweditEmit(const OwEditDocument *doc, const char *path)
{
    owx_puts("owedit: the ring 3 gateway cannot write '");
    owx_puts(path ? path : "(unsaved)");
    owx_puts("'\r\n");
    owx_puts("owedit: emitting the edited document instead (");
    owx_put_u64(doc->size);
    owx_puts(" bytes)\r\n");
    OweditPrintBytes(doc->data, doc->size);
    owx_puts("\r\n-- end of document --\r\n");
}

/* ---------------------------------------------------------------- */
/* Interactive session                                              */
/* ---------------------------------------------------------------- */

/*
 * The Ring 3 console is raw bytes rather than a VT100 stream: no cursor
 * addressing, no erase-line.  The session therefore keeps the whole document
 * on screen and redraws it after every redraw request or newline, which is
 * crude but never lies about the state of the buffer.
 */
static int OweditSession(OwEditDocument *doc, const char *path)
{
    OweditDraw(doc, path);
    owx_puts("-- keys: type to insert, Enter new line, Backspace delete,\r\n");
    owx_puts("        Ctrl+L redraw, Ctrl+S emit, Ctrl+Q quit --\r\n");

    for (;;) {
        int key = owx_getc_wait();

        if (key == (int)OWEDIT_KEY_Q) {
            if (doc->dirty) {
                owx_puts("owedit: quitting with unsaved changes\r\n");
            }
            return 0;
        }
        if (key == (int)OWEDIT_KEY_S) {
            OweditEmit(doc, path);
            continue;
        }
        if (key == (int)OWEDIT_KEY_L) {
            OweditDraw(doc, path);
            continue;
        }
        if (key == (int)'\r' || key == (int)'\n') {
            OweditInsert(doc, (uint8_t)'\n');
            owx_puts("\r\n");
            continue;
        }
        if (key == 0x7F || key == 0x08) {
            /* Erase the character before the cursor on screen as well as in
             * the buffer; the UART console has no backspace, so this is a
             * visible erase character sequence rather than a cursor move. */
            if (doc->cursor > 0U) {
                OweditBackspace(doc);
                owx_puts("\b \b");
            }
            continue;
        }
        if (key >= 0x20 && key < 0x7F) {
            OweditInsert(doc, (uint8_t)key);
            owx_putc((char)key); /* echo: the console is raw, not line-buffered */
            continue;
        }
    }
}

/* ---------------------------------------------------------------- */
/* Selftest                                                         */
/* ---------------------------------------------------------------- */

static int OweditExpect(const char *stage, bool ok)
{
    owx_puts(ok ? "[ok]   " : "[FAIL] ");
    owx_puts(stage);
    owx_puts("\r\n");
    return ok ? 0 : 1;
}

/*
 * Walk the buffer through insert, cursor motion, deletion and saturation.
 * The gateway can read documents but cannot create them, so the editor's own
 * BSS is the only fixture available under emulation.
 */
static int OweditSelfTest(void)
{
    OwEditDocument doc;
    const char *text = "owedit\r\nsecond line\r\n";
    uint32_t i;
    int failures = 0;

    owx_puts("=== owedit selftest ===\r\n");

    OweditInit(&doc);
    failures += OweditExpect("empty buffer", doc.size == 0U && doc.cursor == 0U);
    failures += OweditExpect("capacity is one page",
                             OweditStrLen(text) < (uint32_t)OWEDIT_DOC_CAP);

    for (i = 0; text[i] != '\0'; i++) {
        OweditInsert(&doc, (uint8_t)text[i]);
    }
    failures += OweditExpect("inserts land in order",
                             doc.size == 21U && doc.cursor == 21U);
    failures += OweditExpect("inserts mark the buffer dirty", doc.dirty == true);

    doc.cursor = 8U; /* just after "owedit\r\n" */
    OweditBackspace(&doc);
    failures += OweditExpect("backspace removes one byte", doc.size == 20U);
    failures += OweditExpect("backspace joins the lines", doc.data[7] == (uint8_t)'s');

    doc.cursor = 7U;
    OweditInsert(&doc, (uint8_t)'\n');
    failures += OweditExpect("insert splits at the cursor",
                             doc.data[7] == (uint8_t)'\n' && doc.data[8] == (uint8_t)'s');

    doc.cursor = 0U;
    OweditBackspace(&doc);
    failures += OweditExpect("backspace at the cursor is a no-op", doc.size == 21U);

    OweditInit(&doc);
    for (i = 0; i + 1U < (uint32_t)OWEDIT_DOC_CAP; i++) {
        OweditInsert(&doc, (uint8_t)'x');
    }
    OweditInsert(&doc, (uint8_t)'y'); /* must be refused */
    failures += OweditExpect("saturation refuses the last byte",
                             doc.size == (uint32_t)OWEDIT_DOC_CAP - 1U);

    owx_puts("owedit selftest: ");
    owx_put_i64(failures);
    owx_puts(" failure(s)\r\n");
    return (failures == 0) ? 0 : 1;
}

/* ---------------------------------------------------------------- */
/* Entry point                                                      */
/* ---------------------------------------------------------------- */

int owedit_main(int argc, const char *const *argv)
{
    OwEditDocument doc;
    const char *verb;

    if (argc < 2 || !argv || !argv[1]) {
        OweditPrintUsage();
        return OweditSelfTest();
    }

    verb = argv[1];

    if (OweditStrEqual(verb, "help") ||
        OweditStrEqual(verb, "-h") ||
        OweditStrEqual(verb, "--help")) {
        OweditPrintUsage();
        return 0;
    }
    if (OweditStrEqual(verb, "selftest")) {
        return OweditSelfTest();
    }
    if (OweditStrEqual(verb, "open")) {
        if (argc < 3 || !argv[2]) {
            owx_puts("owedit: open: missing <path>\r\n");
            return 1;
        }
        OweditInit(&doc);
        if (OweditLoad(&doc, argv[2]) != 0) {
            return 1;
        }
        OweditDraw(&doc, argv[2]);
        return 0;
    }
    if (OweditStrEqual(verb, "edit")) {
        const char *path = (argc >= 3 && argv[2]) ? argv[2] : NULL;

        OweditInit(&doc);
        if (path) {
            if (OweditLoad(&doc, path) != 0) {
                owx_puts("owedit: starting an empty document\r\n");
                path = NULL;
            }
        }
        return OweditSession(&doc, path);
    }

    owx_puts("owedit: unknown verb '");
    owx_puts(verb);
    owx_puts("'\r\n");
    OweditPrintUsage();
    return 2;
}
