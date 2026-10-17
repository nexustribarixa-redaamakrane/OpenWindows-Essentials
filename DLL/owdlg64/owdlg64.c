/*
 * owdlg64.c - OpenWindows Dialog/Message Box Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owdlg64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owdlg64_ready = false;
static bool owdlg64_active = false;
static owdlg_hook_fn owdlg64_hook = NULL;
static void *owdlg64_hook_ctx = NULL;
static owdlg_result_t owdlg64_last_result = OWDLG_RESULT_NONE;
static owdlg_kind_t owdlg64_last_kind = OWDLG_KIND_MESSAGE;
static uint32_t owdlg64_count = 0u;

/* ── Internal helpers ─────────────────────────────────────────── */

/* Bounded string copy, always NUL-terminating. */
static void owdlg_copy_string(char *dst, size_t cap, const char *src)
{
    if (!dst || cap == 0u) return;

    size_t i = 0u;
    if (src) {
        for (i = 0u; i + 1u < cap && src[i] != '\0'; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

/* Force a NUL terminator inside the buffer if the producer omitted one. */
static void owdlg_terminate(char *buf, size_t cap)
{
    if (!buf || cap == 0u) return;

    for (size_t i = 0u; i < cap; ++i) {
        if (buf[i] == '\0') return;
    }
    buf[cap - 1u] = '\0';
}

/* Deterministic answer when no host hook is installed. */
static owdlg_result_t owdlg_default_result(owdlg_kind_t kind)
{
    switch (kind) {
    case OWDLG_KIND_MESSAGE:
        return OWDLG_RESULT_OK;
    case OWDLG_KIND_CONFIRM:
        return OWDLG_RESULT_NO;
    case OWDLG_KIND_INPUT:
    case OWDLG_KIND_FILE_OPEN:
    case OWDLG_KIND_FILE_SAVE:
    default:
        return OWDLG_RESULT_CANCEL;
    }
}

/* Run one dialog request through the hook, falling back to defaults. */
static owdlg_result_t owdlg_dispatch(owdlg_request_t *req)
{
    if (!owdlg64_ready || !req) return OWDLG_RESULT_ERROR;

    owdlg_result_t result = owdlg_default_result(req->kind);
    req->result = result;
    owdlg64_active = true;

    if (owdlg64_hook) {
        owdlg_result_t hook_result = owdlg64_hook(owdlg64_hook_ctx, req);
        if (hook_result != OWDLG_RESULT_NONE) {
            result = hook_result;
        } else if (req->result != OWDLG_RESULT_NONE) {
            result = req->result;
        }
    } else if (req->kind == OWDLG_KIND_INPUT &&
               req->default_text && req->input_buf && req->input_cap > 0u) {
        /* Headless fallback: accept the caller's default value. */
        owdlg_copy_string(req->input_buf, req->input_cap, req->default_text);
        result = OWDLG_RESULT_OK;
    }

    req->result = result;
    owdlg64_last_result = result;
    owdlg64_last_kind = req->kind;
    ++owdlg64_count;
    owdlg64_active = false;
    return result;
}

/* ── Implementation ──────────────────────────────────────────── */

bool owdlg_init(owdlg_hook_fn hook, void *ctx)
{
    owdlg64_hook = hook;
    owdlg64_hook_ctx = ctx;
    owdlg64_active = false;
    owdlg64_last_result = OWDLG_RESULT_NONE;
    owdlg64_last_kind = OWDLG_KIND_MESSAGE;
    owdlg64_count = 0u;
    owdlg64_ready = true;
    return owdlg64_ready;
}

owdlg_result_t owdlg_message(const char *title, const char *message)
{
    owdlg_request_t req;

    req.kind = OWDLG_KIND_MESSAGE;
    req.title = title;
    req.message = message;
    req.default_text = NULL;
    req.filter = NULL;
    req.input_buf = NULL;
    req.input_cap = 0u;
    req.result = OWDLG_RESULT_NONE;

    return owdlg_dispatch(&req);
}

owdlg_result_t owdlg_confirm(const char *title, const char *message)
{
    owdlg_request_t req;

    req.kind = OWDLG_KIND_CONFIRM;
    req.title = title;
    req.message = message;
    req.default_text = NULL;
    req.filter = NULL;
    req.input_buf = NULL;
    req.input_cap = 0u;
    req.result = OWDLG_RESULT_NONE;

    return owdlg_dispatch(&req);
}

owdlg_result_t owdlg_input(const char *title, const char *prompt,
                           const char *default_text, char *out, size_t cap)
{
    owdlg_request_t req;

    if (out && cap > 0u) out[0] = '\0';

    req.kind = OWDLG_KIND_INPUT;
    req.title = title;
    req.message = prompt;
    req.default_text = default_text;
    req.filter = NULL;
    req.input_buf = out;
    req.input_cap = cap;
    req.result = OWDLG_RESULT_NONE;

    owdlg_result_t result = owdlg_dispatch(&req);
    if (result == OWDLG_RESULT_OK) owdlg_terminate(out, cap);
    return result;
}

owdlg_result_t owdlg_file_open(const char *title, const char *filter,
                               char *out_path, size_t cap)
{
    owdlg_request_t req;

    if (out_path && cap > 0u) out_path[0] = '\0';

    req.kind = OWDLG_KIND_FILE_OPEN;
    req.title = title;
    req.message = NULL;
    req.default_text = NULL;
    req.filter = filter;
    req.input_buf = out_path;
    req.input_cap = cap;
    req.result = OWDLG_RESULT_NONE;

    owdlg_result_t result = owdlg_dispatch(&req);
    if (result == OWDLG_RESULT_OK) owdlg_terminate(out_path, cap);
    return result;
}

owdlg_result_t owdlg_file_save(const char *title, const char *filter,
                               char *out_path, size_t cap)
{
    owdlg_request_t req;

    if (out_path && cap > 0u) out_path[0] = '\0';

    req.kind = OWDLG_KIND_FILE_SAVE;
    req.title = title;
    req.message = NULL;
    req.default_text = NULL;
    req.filter = filter;
    req.input_buf = out_path;
    req.input_cap = cap;
    req.result = OWDLG_RESULT_NONE;

    owdlg_result_t result = owdlg_dispatch(&req);
    if (result == OWDLG_RESULT_OK) owdlg_terminate(out_path, cap);
    return result;
}

owdlg_result_t owdlg_get_last_result(void)
{
    return owdlg64_last_result;
}

owdlg_kind_t owdlg_get_last_kind(void)
{
    return owdlg64_last_kind;
}

bool owdlg_is_active(void)
{
    return owdlg64_active;
}

uint32_t owdlg_get_count(void)
{
    return owdlg64_count;
}
