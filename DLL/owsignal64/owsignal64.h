/*
 * owsignal64.h - OpenWindows Event & Exception Dispatcher (.owd)
 *
 * Non-POSIX replacement for UNIX signal handlers. Implements structured
 * executive events, BANcode fault dispatch, and Sentinel recovery notifications.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWSIGNAL64_H
#define OWSIGNAL64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_FOUND        0x0011AEE2U
#define OW_ERR_INSUFFICIENT     0x0011AEE4U

typedef uint32_t OW_STATUS;

/* ── Native Event Classes (Non-POSIX) ────────────────────────────── */

#define OW_EVENT_CLASS_SYSTEM_SHUTDOWN   0x0001U
#define OW_EVENT_CLASS_POWER_SUSPEND     0x0002U
#define OW_EVENT_CLASS_DEVICE_ARRIVAL    0x0003U
#define OW_EVENT_CLASS_DEVICE_REMOVAL    0x0004U
#define OW_EVENT_CLASS_MEMORY_PRESSURE   0x0005U
#define OW_EVENT_CLASS_SENTINEL_PANIC    0x0006U
#define OW_EVENT_CLASS_WATCHDOG_BITE     0x0007U

#define OW_EVENT_MAX_HANDLERS            16U

typedef struct _OW_EVENT_CONTEXT {
    uint32_t    EventClass;
    uint32_t    Bancode;
    uint64_t    Parameter1;
    uint64_t    Parameter2;
    uint64_t    Timestamp;
} OW_EVENT_CONTEXT;

typedef void (*OW_EVENT_CALLBACK)(const OW_EVENT_CONTEXT *Context, void *UserContext);

typedef struct _OW_EVENT_SUBSCRIPTION {
    bool                IsActive;
    uint32_t            EventClass;
    OW_EVENT_CALLBACK   Callback;
    void               *UserContext;
} OW_EVENT_SUBSCRIPTION;

/* ── Public API ───────────────────────────────────────────────────── */

OW_STATUS OwEventInitialize(void);

OW_STATUS OwEventSubscribe(uint32_t EventClass,
                           OW_EVENT_CALLBACK Callback,
                           void *UserContext,
                           uint32_t *OutSubscriptionId);

OW_STATUS OwEventUnsubscribe(uint32_t SubscriptionId);

OW_STATUS OwEventDispatch(const OW_EVENT_CONTEXT *Context);

OW_STATUS OwEventRaiseBancode(uint32_t Bancode, uint64_t SubCode);

#endif /* OWSIGNAL64_H */
