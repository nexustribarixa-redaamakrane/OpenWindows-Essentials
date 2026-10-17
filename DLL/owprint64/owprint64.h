/*
 * owprint64.h - OpenWindows Print Spooler Library (.owd)
 *
 * Static printer and job tables plus a FIFO spool queue. Print jobs are
 * submitted against a printer with their documents copied into a fixed
 * per-slot spool buffer; the queue is drained by marking jobs complete or
 * cancelled.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPRINT64_H
#define OWPRINT64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Public types ─────────────────────────────────────────────── */

#define OWPRINT_MAX_PRINTERS  8u    /* printer slots                */
#define OWPRINT_MAX_JOBS     16u    /* spool job slots              */
#define OWPRINT_NAME_MAX     32u    /* stored name length           */
#define OWPRINT_MAX_DOC      4096u  /* spooled bytes per job        */

/* Passing this as the printer id selects the default printer. */
#define OWPRINT_PRINTER_DEFAULT 0u

typedef enum {
    OWPRINT_JOB_QUEUED = 0,
    OWPRINT_JOB_PRINTING,
    OWPRINT_JOB_DONE,
    OWPRINT_JOB_CANCELLED,
    OWPRINT_JOB_ERROR
} owprint_job_state_t;

typedef struct {
    uint32_t id;
    char     name[OWPRINT_NAME_MAX];
    uint32_t page_count;
    bool     used;
    bool     is_default;
} owprint_printer_t;

typedef struct {
    uint32_t            id;
    uint32_t            printer_id;
    char                document[OWPRINT_NAME_MAX];
    uint32_t            bytes;
    uint32_t            sequence;   /* FIFO ordering key            */
    owprint_job_state_t state;
} owprint_job_t;

/* ── Public API ───────────────────────────────────────────────── */

/* Initialize and register a default virtual printer. */
bool owprint_init(void);

/* Register another printer. Returns the assigned id through `out_id`.
 * Only one printer can be the default at a time. */
bool owprint_add_printer(const char *name, bool is_default,
                         uint32_t *out_id);

/* Submit a document to a printer, copying up to OWPRINT_MAX_DOC bytes
 * into the spool. Returns the job id, or 0 on failure. */
uint32_t owprint_submit(uint32_t printer_id, const char *document,
                        const void *data, size_t len);

/* Cancel a queued or printing job. */
bool owprint_cancel(uint32_t job_id);

/* Mark a job as finished. */
bool owprint_complete(uint32_t job_id);

/* Copy up to `cap` pending (queued or printing) jobs in FIFO order.
 * Returns the number copied. */
uint32_t owprint_get_queue(owprint_job_t *out, uint32_t cap);

/* Copy up to `cap` registered printers into `out`. */
uint32_t owprint_enum_printers(owprint_printer_t *out, uint32_t cap);

/* Look up a job by id, including finished and cancelled jobs. */
bool owprint_get_job(uint32_t job_id, owprint_job_t *out);

/* Number of pending jobs. */
uint32_t owprint_get_queue_depth(void);

#endif /* OWPRINT64_H */
