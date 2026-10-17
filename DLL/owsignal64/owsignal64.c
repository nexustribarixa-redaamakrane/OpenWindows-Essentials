/*
 * owsignal64.c - OpenWindows Event & Exception Dispatcher Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owsignal64.h"

static OW_EVENT_SUBSCRIPTION g_Subscriptions[OW_EVENT_MAX_HANDLERS];
static bool g_EventInitialized = false;

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
}

OW_STATUS OwEventInitialize(void)
{
    ow_memzero(g_Subscriptions, sizeof(g_Subscriptions));
    g_EventInitialized = true;
    return OW_SUCCESS;
}

OW_STATUS OwEventSubscribe(uint32_t EventClass,
                           OW_EVENT_CALLBACK Callback,
                           void *UserContext,
                           uint32_t *OutSubscriptionId)
{
    uint32_t i;
    if (!Callback || !OutSubscriptionId) return OW_ERR_NULL_POINTER;
    if (!g_EventInitialized) OwEventInitialize();

    for (i = 0; i < OW_EVENT_MAX_HANDLERS; i++) {
        if (!g_Subscriptions[i].IsActive) {
            g_Subscriptions[i].IsActive = true;
            g_Subscriptions[i].EventClass = EventClass;
            g_Subscriptions[i].Callback = Callback;
            g_Subscriptions[i].UserContext = UserContext;
            *OutSubscriptionId = i;
            return OW_SUCCESS;
        }
    }

    return OW_ERR_INSUFFICIENT;
}

OW_STATUS OwEventUnsubscribe(uint32_t SubscriptionId)
{
    if (SubscriptionId >= OW_EVENT_MAX_HANDLERS) return OW_ERR_INVALID_PARAM;
    if (!g_Subscriptions[SubscriptionId].IsActive) return OW_ERR_NOT_FOUND;

    g_Subscriptions[SubscriptionId].IsActive = false;
    g_Subscriptions[SubscriptionId].Callback = NULL;
    g_Subscriptions[SubscriptionId].UserContext = NULL;
    return OW_SUCCESS;
}

OW_STATUS OwEventDispatch(const OW_EVENT_CONTEXT *Context)
{
    uint32_t i;
    if (!Context) return OW_ERR_NULL_POINTER;
    if (!g_EventInitialized) return OW_SUCCESS;

    for (i = 0; i < OW_EVENT_MAX_HANDLERS; i++) {
        if (g_Subscriptions[i].IsActive &&
            (g_Subscriptions[i].EventClass == Context->EventClass || g_Subscriptions[i].EventClass == 0)) {
            g_Subscriptions[i].Callback(Context, g_Subscriptions[i].UserContext);
        }
    }

    return OW_SUCCESS;
}

OW_STATUS OwEventRaiseBancode(uint32_t Bancode, uint64_t SubCode)
{
    OW_EVENT_CONTEXT ctx;
    ctx.EventClass = OW_EVENT_CLASS_SENTINEL_PANIC;
    ctx.Bancode = Bancode;
    ctx.Parameter1 = SubCode;
    ctx.Parameter2 = 0;
    ctx.Timestamp = 0;

    return OwEventDispatch(&ctx);
}
