/*
 * nvmedrv.c - OpenWindows NVMe Storage Controller (.owc)
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "nvmedrv.h"

/* ── Internal state ───────────────────────────────────────────── */

static bool nvmedrv_initialized = false;

/* ── Stub implementations ─────────────────────────────────────── */
void nvme_init(void)
{
    if (!nvmedrv_initialized) { return; }
    /* TODO: implement nvme_init */
    (void)0;
}
void nvme_identify(void)
{
    if (!nvmedrv_initialized) { return; }
    /* TODO: implement nvme_identify */
    (void)0;
}
void nvme_read(void)
{
    if (!nvmedrv_initialized) { return; }
    /* TODO: implement nvme_read */
    (void)0;
}
void nvme_write(void)
{
    if (!nvmedrv_initialized) { return; }
    /* TODO: implement nvme_write */
    (void)0;
}
void nvme_flush(void)
{
    if (!nvmedrv_initialized) { return; }
    /* TODO: implement nvme_flush */
    (void)0;
}

