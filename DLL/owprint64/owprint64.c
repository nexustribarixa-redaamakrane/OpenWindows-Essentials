/*
 * owprint64.c - OpenWindows Print Spooler Library (.owd)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owprint64.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool owprint64_ready = false;

static owprint_printer_t owprint64_printers[OWPRINT_MAX_PRINTERS];
static uint32_t owprint64_printer_count = 0u;
static uint32_t owprint64_next_printer_id = 1u;
static uint32_t owprint64_default_printer = 0u;

static owprint_job_t owprint64_jobs[OWPRINT_MAX_JOBS];
static uint8_t owprint64_spool[OWPRINT_MAX_JOBS][OWPRINT_MAX_DOC];
static uint32_t owprint64_next_job_id = 1u;
static uint32_t owprint64_next_sequence = 1u;

/* ── Internal helpers ─────────────────────────────────────────── */

static void owprint_copy_bytes(void *dst, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0u; i < len; ++i) d[i] = s[i];
}

static void owprint_copy_string(char *dst, const char *src)
{
    uint32_t i = 0u;

    if (src) {
        for (i = 0u; i + 1u < OWPRINT_NAME_MAX && src[i] != '\0'; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

static owprint_printer_t *owprint_find_printer(uint32_t printer_id)
{
    for (uint32_t i = 0u; i < OWPRINT_MAX_PRINTERS; ++i) {
        if (owprint64_printers[i].used &&
            owprint64_printers[i].id == printer_id) {
            return &owprint64_printers[i];
        }
    }
    return NULL;
}

static int32_t owprint_find_job_slot(uint32_t job_id)
{
    for (uint32_t i = 0u; i < OWPRINT_MAX_JOBS; ++i) {
        if (owprint64_jobs[i].id == job_id && owprint64_jobs[i].id != 0u) {
            return (int32_t)i;
        }
    }
    return -1;
}

static bool owprint_job_pending(const owprint_job_t *job)
{
    return job->id != 0u &&
           (job->state == OWPRINT_JOB_QUEUED ||
            job->state == OWPRINT_JOB_PRINTING);
}

/* Stable insertion sort of jobs by ascending FIFO sequence. */
static void owprint_sort_jobs(owprint_job_t *jobs, uint32_t count)
{
    for (uint32_t i = 1u; i < count; ++i) {
        owprint_job_t key = jobs[i];
        uint32_t j = i;

        while (j > 0u && jobs[j - 1u].sequence > key.sequence) {
            jobs[j] = jobs[j - 1u];
            --j;
        }
        jobs[j] = key;
    }
}

/* ── Implementation ──────────────────────────────────────────── */

bool owprint_init(void)
{
    for (uint32_t i = 0u; i < OWPRINT_MAX_PRINTERS; ++i) {
        owprint64_printers[i].id = 0u;
        owprint64_printers[i].used = false;
        owprint64_printers[i].is_default = false;
        owprint64_printers[i].page_count = 0u;
        owprint64_printers[i].name[0] = '\0';
    }
    for (uint32_t i = 0u; i < OWPRINT_MAX_JOBS; ++i) {
        owprint64_jobs[i].id = 0u;
        owprint64_jobs[i].printer_id = 0u;
        owprint64_jobs[i].bytes = 0u;
        owprint64_jobs[i].sequence = 0u;
        owprint64_jobs[i].state = OWPRINT_JOB_QUEUED;
        owprint64_jobs[i].document[0] = '\0';
    }

    owprint64_printer_count = 0u;
    owprint64_next_printer_id = 1u;
    owprint64_default_printer = 0u;
    owprint64_next_job_id = 1u;
    owprint64_next_sequence = 1u;
    owprint64_ready = true;

    uint32_t default_id = 0u;
    (void)owprint_add_printer("OW Virtual Printer", true, &default_id);
    owprint64_default_printer = default_id;

    return owprint64_ready;
}

bool owprint_add_printer(const char *name, bool is_default,
                         uint32_t *out_id)
{
    if (!owprint64_ready) return false;
    if (owprint64_printer_count >= OWPRINT_MAX_PRINTERS) return false;

    for (uint32_t i = 0u; i < OWPRINT_MAX_PRINTERS; ++i) {
        if (owprint64_printers[i].used) continue;

        owprint64_printers[i].id = owprint64_next_printer_id++;
        if (owprint64_next_printer_id == 0u) owprint64_next_printer_id = 1u;
        owprint64_printers[i].page_count = 0u;
        owprint64_printers[i].used = true;
        owprint_copy_string(owprint64_printers[i].name, name);

        if (is_default) {
            for (uint32_t j = 0u; j < OWPRINT_MAX_PRINTERS; ++j) {
                owprint64_printers[j].is_default = (j == i);
            }
            owprint64_default_printer = owprint64_printers[i].id;
        } else {
            owprint64_printers[i].is_default = false;
        }

        ++owprint64_printer_count;
        if (out_id) *out_id = owprint64_printers[i].id;
        return true;
    }

    return false;
}

uint32_t owprint_submit(uint32_t printer_id, const char *document,
                        const void *data, size_t len)
{
    if (!owprint64_ready) return 0u;
    if (len > (size_t)OWPRINT_MAX_DOC) return 0u;
    if (len != 0u && !data) return 0u;

    uint32_t resolved = (printer_id == OWPRINT_PRINTER_DEFAULT)
                            ? owprint64_default_printer
                            : printer_id;
    if (!owprint_find_printer(resolved)) return 0u;

    for (uint32_t i = 0u; i < OWPRINT_MAX_JOBS; ++i) {
        if (owprint64_jobs[i].id != 0u) continue;

        if (len != 0u) {
            owprint_copy_bytes(owprint64_spool[i], data, len);
        }

        owprint64_jobs[i].id = owprint64_next_job_id++;
        if (owprint64_next_job_id == 0u) owprint64_next_job_id = 1u;
        owprint64_jobs[i].printer_id = resolved;
        owprint64_jobs[i].bytes = (uint32_t)len;
        owprint64_jobs[i].sequence = owprint64_next_sequence++;
        owprint64_jobs[i].state = OWPRINT_JOB_QUEUED;
        owprint_copy_string(owprint64_jobs[i].document, document);

        return owprint64_jobs[i].id;
    }

    return 0u;
}

bool owprint_cancel(uint32_t job_id)
{
    int32_t slot = owprint_find_job_slot(job_id);
    if (slot < 0) return false;
    if (!owprint_job_pending(&owprint64_jobs[slot])) return false;

    owprint64_jobs[slot].state = OWPRINT_JOB_CANCELLED;
    return true;
}

bool owprint_complete(uint32_t job_id)
{
    int32_t slot = owprint_find_job_slot(job_id);
    if (slot < 0) return false;
    if (!owprint_job_pending(&owprint64_jobs[slot])) return false;

    owprint64_jobs[slot].state = OWPRINT_JOB_DONE;
    return true;
}

uint32_t owprint_get_queue(owprint_job_t *out, uint32_t cap)
{
    if (out && cap > 0u) {
        uint32_t copied = 0u;

        for (uint32_t i = 0u; i < OWPRINT_MAX_JOBS && copied < cap; ++i) {
            if (!owprint_job_pending(&owprint64_jobs[i])) continue;
            out[copied] = owprint64_jobs[i];
            ++copied;
        }

        owprint_sort_jobs(out, copied);
        return copied;
    }

    return owprint_get_queue_depth();
}

uint32_t owprint_enum_printers(owprint_printer_t *out, uint32_t cap)
{
    if (!out || cap == 0u) return owprint64_printer_count;

    uint32_t copied = 0u;
    for (uint32_t i = 0u; i < OWPRINT_MAX_PRINTERS && copied < cap; ++i) {
        if (!owprint64_printers[i].used) continue;
        out[copied] = owprint64_printers[i];
        ++copied;
    }
    return copied;
}

bool owprint_get_job(uint32_t job_id, owprint_job_t *out)
{
    int32_t slot = owprint_find_job_slot(job_id);
    if (slot < 0 || !out) return false;

    *out = owprint64_jobs[slot];
    return true;
}

uint32_t owprint_get_queue_depth(void)
{
    uint32_t depth = 0u;

    for (uint32_t i = 0u; i < OWPRINT_MAX_JOBS; ++i) {
        if (owprint_job_pending(&owprint64_jobs[i])) ++depth;
    }
    return depth;
}
