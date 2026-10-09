/*
 * fltrmgr.h - Filter Manager Driver
 *
 * Intercepts I/O requests below the storage stack via an ordered chain
 * of filter callbacks that can inspect, modify, or complete requests
 * before they reach the underlying hardware driver.
 *
 * C99 freestanding. No heap allocation. All memory is caller-provided.
 * Binary output: fltrmgr.owc
 */

#ifndef FLTRMGR_H
#define FLTRMGR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Include OWC format for BANcode sentinel cross-references */
#include "../../Extensions/owc_format.h"

/* ------------------------------------------------------------------ */
/*  Constants                                                          */
/* ------------------------------------------------------------------ */

#define FLMGR_MAX_FILTERS       32u
#define FLMGR_MAX_PENDING       256u
#define FLMGR_MAX_FILTER_NAME   32u
#define FLMGR_INVALID_ID        0xFFFFFFFFu

/* ------------------------------------------------------------------ */
/*  Request Types                                                      */
/* ------------------------------------------------------------------ */

/* Filter names are NUL-terminated SUTF-8 transport strings.           */

typedef enum {
    FLMGR_REQ_READ       = 0x00u,
    FLMGR_REQ_WRITE      = 0x01u,
    FLMGR_REQ_FLUSH      = 0x02u,
    FLMGR_REQ_DEVCTL     = 0x03u,
    FLMGR_REQ_PNP        = 0x04u
} fltrmgr_req_type_t;

/* ------------------------------------------------------------------ */
/*  Request Status                                                     */
/* ------------------------------------------------------------------ */

typedef enum {
    FLMGR_REQ_PENDING      = 0x00u,
    FLMGR_REQ_IN_PROGRESS  = 0x01u,
    FLMGR_REQ_COMPLETED    = 0x02u,
    FLMGR_REQ_FAILED       = 0x03u,
    FLMGR_REQ_CANCELLED    = 0x04u
} fltrmgr_req_status_t;

/* ------------------------------------------------------------------ */
/*  Manager Status — BANcode mapped                                    */
/*  B+ (0x0011A000-0x0011A77F): Fatal faults                          */
/*  W+ (0x0011A800-0x0011ABFF): Non-fatal warnings                    */
/*  C+ (0x0011AC00-0x0011ADFF): Communication / protocol              */
/*  S+ (0x0011AE00-0x0011AEFF): Soft / recoverable                    */
/* ------------------------------------------------------------------ */

typedef uint32_t fltrmgr_status_t;

#define FLMGR_OK                         0x00000000u  /* Success              */
/* B+ Fatal */
#define FLMGR_ERR_CHAIN_BROKEN           0x0011A000u  /* Filter chain broken  */
#define FLMGR_ERR_REQUEST_FULL           0x0011A001u  /* Request pool full    */
#define FLMGR_ERR_FILTER_FULL            0x0011A002u  /* Filter table full    */
#define FLMGR_ERR_ALREADY_ACTIVE         0x0011A003u  /* Already active       */
/* W+ Warning */
#define FLMGR_ERR_NO_FILTERS             0x0011A800u  /* No filters attached  */
/* C+ Communication */
#define FLMGR_ERR_REQUEST_NOT_PENDING    0x0011AC00u  /* Request state bad    */
/* S+ Soft */
#define FLMGR_ERR_NOT_FOUND              0x0011AE00u  /* Entry not found      */
#define FLMGR_ERR_INVALID_PARAM          0x0011AE01u  /* Bad parameter        */

/* ------------------------------------------------------------------ */
/*  Request Flags                                                      */
/* ------------------------------------------------------------------ */

#define FLMGR_FLAG_NONE         0x00000000u
#define FLMGR_FLAG_BYPASS       0x00000001u
#define FLMGR_FLAG_HIGH_PRIO    0x00000002u
#define FLMGR_FLAG_NO_COMPLETE  0x00000004u

/* ------------------------------------------------------------------ */
/*  Forward Declarations                                               */
/* ------------------------------------------------------------------ */

typedef struct fltrmgr_request fltrmgr_request_t;
typedef struct fltrmgr_filter  fltrmgr_filter_t;
typedef struct fltrmgr_context fltrmgr_context_t;

/* ------------------------------------------------------------------ */
/*  Filter Callback Signature                                          */
/*  Called for each request walking the chain. Return FLMGR_OK to      */
/*  continue, or any error to abort the chain.                        */
/* ------------------------------------------------------------------ */

typedef fltrmgr_status_t (*fltrmgr_filter_fn)(fltrmgr_request_t *req,
                                              void *filter_ctx);

/* ------------------------------------------------------------------ */
/*  Completion Callback Signature                                      */
/* ------------------------------------------------------------------ */

typedef void (*fltrmgr_completion_fn)(fltrmgr_request_t *req, void *ctx);

/* ------------------------------------------------------------------ */
/*  I/O Request Descriptor                                             */
/* ------------------------------------------------------------------ */

struct fltrmgr_request {
    uint32_t                id;             /* Unique request ID        */
    fltrmgr_req_type_t      type;           /* Operation type           */
    uint64_t                lba;            /* Logical block address    */
    uint32_t                sector_count;   /* Number of sectors        */
    void                   *buffer;         /* Data buffer              */
    volatile fltrmgr_req_status_t status;   /* Current request status  */
    uint32_t                flags;          /* FLMGR_FLAG_*             */
    fltrmgr_completion_fn   completion_fn;  /* Post-chain callback      */
    void                   *completion_ctx; /* Completion user context  */
    struct fltrmgr_request *next;           /* Linked-list / free-chain */
};

/* ------------------------------------------------------------------ */
/*  Registered Filter                                                  */
/* ------------------------------------------------------------------ */

struct fltrmgr_filter {
    char                    name[FLMGR_MAX_FILTER_NAME];
    uint32_t                priority;       /* Lower = closer to HW    */
    fltrmgr_filter_fn       callback;       /* Filter entry point      */
    void                   *context;        /* Filter-private context  */
    bool                    active;         /* Currently enabled?      */
};

/* ------------------------------------------------------------------ */
/*  Statistics                                                         */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t total_submitted;
    uint32_t total_completed;
    uint32_t total_failed;
    uint32_t total_cancelled;
    uint32_t active_filters;
} fltrmgr_stats_t;

/* ------------------------------------------------------------------ */
/*  Manager Context (caller-provided, zero before init)                */
/* ------------------------------------------------------------------ */

struct fltrmgr_context {
    fltrmgr_filter_t        filters[FLMGR_MAX_FILTERS];
    uint32_t                filter_count;
    fltrmgr_request_t       requests[FLMGR_MAX_PENDING];
    uint32_t                request_count;
    uint32_t                next_id;        /* Monotonic ID counter    */
    fltrmgr_stats_t         stats;

    /* BANcode trap range cross-reference for error reporting context.
     * Trap slots 0x7FFFFFF0..0x7FFFFFFE map into this range when a
     * sentinel violation is detected during filter processing. */
    uint32_t                bancode_err_trap_base; /* OWC_TRAP_BASE   */
};

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */

fltrmgr_status_t fltrmgr_init(
    fltrmgr_context_t *ctx,
    fltrmgr_filter_t  *filter_buf,
    uint32_t           filter_cap,
    fltrmgr_request_t *req_buf,
    uint32_t           req_cap
);

fltrmgr_status_t fltrmgr_register_filter(
    fltrmgr_context_t   *ctx,
    const char          *name,
    uint32_t             priority,
    fltrmgr_filter_fn    callback,
    void                *filter_ctx
);

fltrmgr_status_t fltrmgr_unregister_filter(
    fltrmgr_context_t *ctx,
    const char        *name
);

fltrmgr_status_t fltrmgr_submit_request(
    fltrmgr_context_t   *ctx,
    fltrmgr_request_t   *req
);

fltrmgr_status_t fltrmgr_cancel_request(
    fltrmgr_context_t *ctx,
    uint32_t           req_id
);

fltrmgr_status_t fltrmgr_get_stats(
    const fltrmgr_context_t *ctx,
    fltrmgr_stats_t         *stats_out
);

fltrmgr_status_t fltrmgr_reset(
    fltrmgr_context_t *ctx
);

fltrmgr_status_t fltrmgr_flush_chain(
    fltrmgr_context_t *ctx
);

#endif /* FLTRMGR_H */
