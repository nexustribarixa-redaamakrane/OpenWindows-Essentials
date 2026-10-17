/*
 * owthread64.c - OpenWindows Executive Thread Manager Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owthread64.h"

static OW_THREAD_DESCRIPTOR g_ThreadTable[OW_MAX_SYSTEM_THREADS];
static bool g_ThreadSubsystemInit = false;

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
}

OW_STATUS OwThreadInitialize(void)
{
    ow_memzero(g_ThreadTable, sizeof(g_ThreadTable));
    g_ThreadSubsystemInit = true;
    return OW_SUCCESS;
}

OW_STATUS OwThreadCreate(uint32_t ProcessId,
                         OW_THREAD_START_ROUTINE EntryPoint,
                         void *Context,
                         uint32_t Priority,
                         OW_THREAD_HANDLE *OutHandle)
{
    uint32_t i;
    if (!EntryPoint || !OutHandle) return OW_ERR_NULL_POINTER;
    if (!g_ThreadSubsystemInit) OwThreadInitialize();

    for (i = 0; i < OW_MAX_SYSTEM_THREADS; i++) {
        if (!g_ThreadTable[i].InUse) {
            g_ThreadTable[i].InUse = true;
            g_ThreadTable[i].ThreadId = i + 100;
            g_ThreadTable[i].ProcessId = ProcessId;
            g_ThreadTable[i].Priority = Priority;
            g_ThreadTable[i].RemainingQuantum = 10; /* default 10 ticks */
            g_ThreadTable[i].State = OW_EX_THR_READY;
            g_ThreadTable[i].EntryFunction = EntryPoint;
            g_ThreadTable[i].Context = Context;
            g_ThreadTable[i].TotalExecutedTicks = 0;
            *OutHandle = i;
            return OW_SUCCESS;
        }
    }

    return OW_ERR_INSUFFICIENT;
}

OW_STATUS OwThreadSuspend(OW_THREAD_HANDLE Handle)
{
    if (Handle >= OW_MAX_SYSTEM_THREADS || !g_ThreadTable[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }

    g_ThreadTable[Handle].State = OW_EX_THR_BLOCKED;
    return OW_SUCCESS;
}

OW_STATUS OwThreadResume(OW_THREAD_HANDLE Handle)
{
    if (Handle >= OW_MAX_SYSTEM_THREADS || !g_ThreadTable[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }

    g_ThreadTable[Handle].State = OW_EX_THR_READY;
    return OW_SUCCESS;
}

OW_STATUS OwThreadTerminate(OW_THREAD_HANDLE Handle, uint32_t ExitCode)
{
    (void)ExitCode;
    if (Handle >= OW_MAX_SYSTEM_THREADS || !g_ThreadTable[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }

    g_ThreadTable[Handle].State = OW_EX_THR_TERMINATED;
    g_ThreadTable[Handle].InUse = false;
    return OW_SUCCESS;
}

OW_STATUS OwThreadYieldExecution(void)
{
    /* Cooperative quantum yield */
    return OW_SUCCESS;
}

OW_STATUS OwThreadQueryInfo(OW_THREAD_HANDLE Handle, OW_THREAD_DESCRIPTOR *OutDesc)
{
    if (!OutDesc) return OW_ERR_NULL_POINTER;
    if (Handle >= OW_MAX_SYSTEM_THREADS || !g_ThreadTable[Handle].InUse) {
        return OW_ERR_INVALID_PARAM;
    }

    *OutDesc = g_ThreadTable[Handle];
    return OW_SUCCESS;
}
