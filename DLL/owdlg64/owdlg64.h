/*
 * owdlg64.h - OpenWindows Dialog/Message Box Library (.owd)
 *
 * Models modal dialogs (message, confirm, text input, file open/save) and
 * dispatches them to an injected prompt/render hook. The host embeds a
 * hook that draws the dialog and returns the user's choice; without a
 * hook the library falls back to deterministic defaults so headless code
 * still makes progress.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWDLG64_H
#define OWDLG64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWDLG_TEXT_MAX 96u    /* stored title/message length */

typedef enum {
    OWDLG_KIND_MESSAGE = 0,   /* informational              */
    OWDLG_KIND_CONFIRM,       /* yes / no question          */
    OWDLG_KIND_INPUT,         /* single-line text entry     */
    OWDLG_KIND_FILE_OPEN,     /* open-file prompt           */
    OWDLG_KIND_FILE_SAVE      /* save-file prompt           */
} owdlg_kind_t;

typedef enum {
    OWDLG_RESULT_NONE = 0,
    OWDLG_RESULT_OK,
    OWDLG_RESULT_CANCEL,
    OWDLG_RESULT_YES,
    OWDLG_RESULT_NO,
    OWDLG_RESULT_ERROR
} owdlg_result_t;

/* One dialog request handed to the hook. The hook may write into
 * `input_buf` (NUL-terminated) for input and file dialogs and must return
 * the chosen result. */
typedef struct {
    owdlg_kind_t   kind;
    const char    *title;
    const char    *message;       /* prompt / body text            */
    const char    *default_text;  /* pre-filled value, may be NULL */
    const char    *filter;        /* file filter, may be NULL      */
    char          *input_buf;     /* result buffer, may be NULL    */
    size_t         input_cap;     /* bytes available in input_buf  */
    owdlg_result_t result;
} owdlg_request_t;

typedef owdlg_result_t (*owdlg_hook_fn)(void *ctx, owdlg_request_t *req);

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize and register the dialog hook. `hook` may be NULL, in which
 * case deterministic fallback results are used. */
bool owdlg_init(owdlg_hook_fn hook, void *ctx);

/* Show an informational message box. */
owdlg_result_t owdlg_message(const char *title, const char *message);

/* Show a yes/no confirmation. Fallback answer is NO. */
owdlg_result_t owdlg_confirm(const char *title, const char *message);

/* Prompt for a single line of text into `out` (size `cap`). */
owdlg_result_t owdlg_input(const char *title, const char *prompt,
                           const char *default_text, char *out, size_t cap);

/* Prompt for a path to open. */
owdlg_result_t owdlg_file_open(const char *title, const char *filter,
                               char *out_path, size_t cap);

/* Prompt for a path to save. */
owdlg_result_t owdlg_file_save(const char *title, const char *filter,
                               char *out_path, size_t cap);

/* Result of the most recent dialog. */
owdlg_result_t owdlg_get_last_result(void);

/* Kind of the most recent dialog. */
owdlg_kind_t owdlg_get_last_kind(void);

/* True while a hook call is in progress. */
bool owdlg_is_active(void);

/* Number of dialogs dispatched since owdlg_init(). */
uint32_t owdlg_get_count(void);

#endif /* OWDLG64_H */
