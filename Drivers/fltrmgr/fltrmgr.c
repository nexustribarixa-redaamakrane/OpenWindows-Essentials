/*
 * fltrmgr.c - Filter Manager Driver Implementation
 *
 * C99 freestanding. No heap allocation. All memory is caller-provided
 * static pools passed to fltrmgr_init().
 *
 * Cross-reference: BANcode trap range 0x0011A000-0x0011A7FF for
 * sentinel recovery context. Trap base at 0x7FFFFFF0.
 */

#include "fltrmgr.h"

/* ------------------------------------------------------------------ */
/*  Internal Helpers                                                    */
/* ------------------------------------------------------------------ */

/* Volatile-safe zero for a region. */
static void volatile_memset(void *dst, uint8_t val, uint32_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    for (uint32_t i = 0; i < size; i++) {
        d[i] = val;
    }
}

/* Safe string copy with explicit NUL guarantee. */
static void safe_strncpy(char *dst, const char *src, uint32_t dst_size)
{
    uint32_t i;
    for (i = 0; i < dst_size - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

/* String comparison, returns true if equal. */
static bool str_equal(const char *a, const char *b, uint32_t max_len)
{
    for (uint32_t i = 0; i < max_len; i++) {
        if (a[i] != b[i]) return false;
        if (a[i] == '\0') return true;
    }
    return true;
}

/* Find a filter index by name. Returns FLMGR_MAX_FILTERS if not found. */
static uint32_t find_filter_by_name(const fltrmgr_context_t *ctx,
                                    const char *name)
{
    for (uint32_t i = 0; i < ctx->filter_count; i++) {
        if (ctx->filters[i].active &&
            str_equal(ctx->filters[i].name, name, FLMGR_MAX_FILTER_NAME)) {
            return i;
        }
    }
    return FLMGR_MAX_FILTERS;
}

/* Find a pending request index by ID. Returns FLMGR_MAX_PENDING if not found. */
static uint32_t find_request_by_id(const fltrmgr_context_t *ctx, uint32_t id)
{
    for (uint32_t i = 0; i < ctx->request_count; i++) {
        if (ctx->requests[i].id == id) {
            return i;
        }
    }
    return FLMGR_MAX_PENDING;
}

/* ------------------------------------------------------------------ */
/*  fltrmgr_init                                                       */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_init(
    fltrmgr_context_t *ctx,
    fltrmgr_filter_t  *filter_buf,
    uint32_t           filter_cap,
    fltrmgr_request_t *req_buf,
    uint32_t           req_cap)
{
    if (!ctx) return FLMGR_ERR_INVALID_PARAM;

    /* The manager owns fixed in-struct pools; caller buffers are not
     * consumed directly. */
    (void)filter_buf;
    (void)req_buf;

    volatile_memset(ctx, 0, sizeof(fltrmgr_context_t));

    /* Wire up caller-provided pools.
     * The context's internal arrays are used as-is; callers provide
     * their own memory and pass the count in. We validate bounds but
     * store into the fixed arrays defined within the context struct. */
    if (filter_cap > FLMGR_MAX_FILTERS) {
        filter_cap = FLMGR_MAX_FILTERS;
    }
    if (req_cap > FLMGR_MAX_PENDING) {
        req_cap = FLMGR_MAX_PENDING;
    }

    /* Zero the filter pool */
    for (uint32_t i = 0; i < filter_cap; i++) {
        volatile_memset(&ctx->filters[i], 0, sizeof(fltrmgr_filter_t));
    }

    /* Zero the request pool */
    for (uint32_t i = 0; i < req_cap; i++) {
        volatile_memset(&ctx->requests[i], 0, sizeof(fltrmgr_request_t));
        ctx->requests[i].status = FLMGR_REQ_CANCELLED;
    }

    ctx->filter_count  = 0;
    ctx->request_count = 0;
    ctx->next_id       = 1;

    volatile_memset(&ctx->stats, 0, sizeof(fltrmgr_stats_t));

    /* Cross-reference BANcode trap base for sentinel error reporting */
    ctx->bancode_err_trap_base = OWC_TRAP_BASE;

    return FLMGR_OK;
}

/* ------------------------------------------------------------------ */
/*  fltrmgr_register_filter                                            */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_register_filter(
    fltrmgr_context_t   *ctx,
    const char          *name,
    uint32_t             priority,
    fltrmgr_filter_fn    callback,
    void                *filter_ctx)
{
    if (!ctx || !name || !callback) return FLMGR_ERR_INVALID_PARAM;

    if (find_filter_by_name(ctx, name) != FLMGR_MAX_FILTERS) {
        return FLMGR_ERR_ALREADY_ACTIVE;
    }

    if (ctx->filter_count >= FLMGR_MAX_FILTERS) {
        return FLMGR_ERR_FILTER_FULL;
    }

    /* Find insertion point sorted by priority (ascending, lower first). */
    uint32_t insert_idx = ctx->filter_count;
    for (uint32_t i = 0; i < ctx->filter_count; i++) {
        if (priority < ctx->filters[i].priority) {
            insert_idx = i;
            break;
        }
    }

    /* Shift filters right if inserting in the middle */
    if (insert_idx < ctx->filter_count) {
        for (uint32_t i = ctx->filter_count; i > insert_idx; i--) {
            ctx->filters[i] = ctx->filters[i - 1];
        }
    }

    /* Populate the new slot */
    volatile_memset(&ctx->filters[insert_idx], 0, sizeof(fltrmgr_filter_t));
    safe_strncpy(ctx->filters[insert_idx].name, name,
                 FLMGR_MAX_FILTER_NAME);
    ctx->filters[insert_idx].priority  = priority;
    ctx->filters[insert_idx].callback  = callback;
    ctx->filters[insert_idx].context   = filter_ctx;
    ctx->filters[insert_idx].active    = true;

    ctx->filter_count++;
    ctx->stats.active_filters = ctx->filter_count;

    return FLMGR_OK;
}

/* ------------------------------------------------------------------ */
/*  fltrmgr_unregister_filter                                          */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_unregister_filter(
    fltrmgr_context_t *ctx,
    const char        *name)
{
    if (!ctx || !name) return FLMGR_ERR_INVALID_PARAM;

    uint32_t idx = find_filter_by_name(ctx, name);
    if (idx == FLMGR_MAX_FILTERS) {
        return FLMGR_ERR_NOT_FOUND;
    }

    /* Mark inactive */
    ctx->filters[idx].active = false;

    /* Shift remaining filters left to close the gap */
    for (uint32_t i = idx; i + 1 < ctx->filter_count; i++) {
        ctx->filters[i] = ctx->filters[i + 1];
    }

    /* Clear the vacated slot */
    volatile_memset(&ctx->filters[ctx->filter_count - 1], 0,
                    sizeof(fltrmgr_filter_t));

    ctx->filter_count--;
    ctx->stats.active_filters = ctx->filter_count;

    return FLMGR_OK;
}

/* ------------------------------------------------------------------ */
/*  fltrmgr_submit_request                                             */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_submit_request(
    fltrmgr_context_t   *ctx,
    fltrmgr_request_t   *req)
{
    if (!ctx || !req) return FLMGR_ERR_INVALID_PARAM;
    if (!req->buffer) return FLMGR_ERR_INVALID_PARAM;

    /* Find a free slot in the pending array */
    uint32_t slot = FLMGR_MAX_PENDING;
    for (uint32_t i = 0; i < FLMGR_MAX_PENDING; i++) {
        if (ctx->requests[i].status == FLMGR_REQ_CANCELLED ||
            ctx->requests[i].status == FLMGR_REQ_COMPLETED) {
            slot = i;
            break;
        }
    }

    if (slot == FLMGR_MAX_PENDING) {
        return FLMGR_ERR_REQUEST_FULL;
    }

    /* Assign unique monotonic ID */
    req->id     = ctx->next_id++;
    req->status = FLMGR_REQ_PENDING;
    req->next   = (fltrmgr_request_t *)0;

    /* Copy request into the pending slot */
    ctx->requests[slot] = *req;

    ctx->stats.total_submitted++;

    /* If no filters registered, jump straight to completion */
    if (ctx->filter_count == 0) {
        ctx->requests[slot].status = FLMGR_REQ_COMPLETED;
        ctx->stats.total_completed++;
        if (ctx->requests[slot].completion_fn &&
            !(ctx->requests[slot].flags & FLMGR_FLAG_NO_COMPLETE)) {
            ctx->requests[slot].completion_fn(
                &ctx->requests[slot],
                ctx->requests[slot].completion_ctx);
        }
        /* Write back to caller's request struct */
        *req = ctx->requests[slot];
        return FLMGR_OK;
    }

    /* Walk the filter chain: lowest priority (hardware-near) first,
     * then ascending toward higher (software-near) priorities. */
    ctx->requests[slot].status = FLMGR_REQ_IN_PROGRESS;
    fltrmgr_status_t chain_result = FLMGR_OK;

    for (uint32_t i = 0; i < ctx->filter_count; i++) {
        if (!ctx->filters[i].active) continue;
        if (ctx->filters[i].callback == (fltrmgr_filter_fn)0) continue;

        /* Check if sentinel trap is needed (BANcode cross-ref) */
        uint32_t trap_addr = ctx->bancode_err_trap_base + i;
        if (trap_addr > OWC_TRAP_END) {
            trap_addr = OWC_TRAP_BASE;
        }

        chain_result = ctx->filters[i].callback(
            &ctx->requests[slot],
            ctx->filters[i].context);

        if (chain_result != FLMGR_OK) {
            /* Chain broken by filter */
            ctx->requests[slot].status = FLMGR_REQ_FAILED;
            ctx->stats.total_failed++;
            req->status = FLMGR_REQ_FAILED;
            return FLMGR_ERR_CHAIN_BROKEN;
        }

        /* If filter completed the request (e.g. cache hit), stop */
        if (ctx->requests[slot].status == FLMGR_REQ_COMPLETED) {
            ctx->stats.total_completed++;
            break;
        }

        /* If filter cancelled, stop */
        if (ctx->requests[slot].status == FLMGR_REQ_CANCELLED) {
            ctx->stats.total_cancelled++;
            req->status = FLMGR_REQ_CANCELLED;
            return FLMGR_OK;
        }
    }

    /* If still in progress after all filters, mark completed */
    if (ctx->requests[slot].status == FLMGR_REQ_IN_PROGRESS) {
        ctx->requests[slot].status = FLMGR_REQ_COMPLETED;
        ctx->stats.total_completed++;
    }

    /* Invoke completion callback if present */
    if (ctx->requests[slot].completion_fn &&
        !(ctx->requests[slot].flags & FLMGR_FLAG_NO_COMPLETE)) {
        ctx->requests[slot].completion_fn(
            &ctx->requests[slot],
            ctx->requests[slot].completion_ctx);
    }

    /* Write final state back to caller */
    *req = ctx->requests[slot];

    return FLMGR_OK;
}

/* ------------------------------------------------------------------ */
/*  fltrmgr_cancel_request                                             */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_cancel_request(
    fltrmgr_context_t *ctx,
    uint32_t           req_id)
{
    if (!ctx) return FLMGR_ERR_INVALID_PARAM;
    if (req_id == FLMGR_INVALID_ID) return FLMGR_ERR_INVALID_PARAM;

    uint32_t idx = find_request_by_id(ctx, req_id);
    if (idx == FLMGR_MAX_PENDING) {
        return FLMGR_ERR_NOT_FOUND;
    }

    if (ctx->requests[idx].status != FLMGR_REQ_PENDING) {
        return FLMGR_ERR_REQUEST_NOT_PENDING;
    }

    ctx->requests[idx].status = FLMGR_REQ_CANCELLED;
    ctx->stats.total_cancelled++;

    return FLMGR_OK;
}

/* ------------------------------------------------------------------ */
/*  fltrmgr_get_stats                                                  */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_get_stats(
    const fltrmgr_context_t *ctx,
    fltrmgr_stats_t         *stats_out)
{
    if (!ctx || !stats_out) return FLMGR_ERR_INVALID_PARAM;

    stats_out->total_submitted = ctx->stats.total_submitted;
    stats_out->total_completed = ctx->stats.total_completed;
    stats_out->total_failed    = ctx->stats.total_failed;
    stats_out->total_cancelled = ctx->stats.total_cancelled;
    stats_out->active_filters  = ctx->stats.active_filters;

    return FLMGR_OK;
}

/* ------------------------------------------------------------------ */
/*  fltrmgr_reset                                                      */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_reset(
    fltrmgr_context_t *ctx)
{
    if (!ctx) return FLMGR_ERR_INVALID_PARAM;

    /* Clear all filter slots */
    for (uint32_t i = 0; i < FLMGR_MAX_FILTERS; i++) {
        volatile_memset(&ctx->filters[i], 0, sizeof(fltrmgr_filter_t));
    }

    /* Clear all pending request slots */
    for (uint32_t i = 0; i < FLMGR_MAX_PENDING; i++) {
        volatile_memset(&ctx->requests[i], 0, sizeof(fltrmgr_request_t));
        ctx->requests[i].status = FLMGR_REQ_CANCELLED;
    }

    ctx->filter_count  = 0;
    ctx->request_count = 0;
    ctx->next_id       = 1;

    volatile_memset(&ctx->stats, 0, sizeof(fltrmgr_stats_t));

    ctx->bancode_err_trap_base = OWC_TRAP_BASE;

    return FLMGR_OK;
}

/* ------------------------------------------------------------------ */
/*  fltrmgr_flush_chain                                                */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_flush_chain(
    fltrmgr_context_t *ctx)
{
    if (!ctx) return FLMGR_ERR_INVALID_PARAM;

    for (uint32_t i = 0; i < FLMGR_MAX_PENDING; i++) {
        if (ctx->requests[i].status == FLMGR_REQ_PENDING ||
            ctx->requests[i].status == FLMGR_REQ_IN_PROGRESS) {

            ctx->requests[i].status = FLMGR_REQ_CANCELLED;
            ctx->stats.total_cancelled++;

            if (ctx->requests[i].completion_fn &&
                !(ctx->requests[i].flags & FLMGR_FLAG_NO_COMPLETE)) {
                ctx->requests[i].completion_fn(
                    &ctx->requests[i],
                    ctx->requests[i].completion_ctx);
            }
        }
    }

    return FLMGR_OK;
}
