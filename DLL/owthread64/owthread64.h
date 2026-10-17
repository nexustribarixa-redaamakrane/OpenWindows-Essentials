/*
 * owthread64.h - OpenWindows Executive Thread Manager (.owd)
 *
 * Implements non-POSIX NT-like executive thread management conforming
 * to OpenWindows ps/ow_ps.h subsystem design.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWTHREAD64_H
#define OWTHREAD64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_FOUND        0x0011AEE2U
#define OW_ERR_INSUFFICIENT     0x0011AEE4U

typedef uint32_t OW_STATUS;
typedef uint32_t OW_THREAD_HANDLE;

#define OW_MAX_SYSTEM_THREADS   32U
#define OW_THREAD_INVALID_HANDLE 0xFFFFFFFFU

typedef enum _OW_EXECUTIVE_THREAD_STATE {
    OW_EX_THR_DORMANT = 0,
    OW_EX_THR_READY = 1,
    OW_EX_THR_RUNNING = 2,
    OW_EX_THR_BLOCKED = 3,
    OW_EX_THR_TERMINATED = 4
} OW_EXECUTIVE_THREAD_STATE;

typedef void (*OW_THREAD_START_ROUTINE)(void *Context);

typedef struct _OW_THREAD_DESCRIPTOR {
    bool                        InUse;
    uint32_t                    ThreadId;
    uint32_t                    ProcessId;
    uint32_t                    Priority;
    uint32_t                    RemainingQuantum;
    OW_EXECUTIVE_THREAD_STATE   State;
    OW_THREAD_START_ROUTINE     EntryFunction;
    void                       *Context;
    uint64_t                    TotalExecutedTicks;
} OW_THREAD_DESCRIPTOR;

/* ── Public API ───────────────────────────────────────────────────── */

OW_STATUS OwThreadInitialize(void);

OW_STATUS OwThreadCreate(uint32_t ProcessId,
                         OW_THREAD_START_ROUTINE EntryPoint,
                         void *Context,
                         uint32_t Priority,
                         OW_THREAD_HANDLE *OutHandle);

OW_STATUS OwThreadSuspend(OW_THREAD_HANDLE Handle);

OW_STATUS OwThreadResume(OW_THREAD_HANDLE Handle);

OW_STATUS OwThreadTerminate(OW_THREAD_HANDLE Handle, uint32_t ExitCode);

OW_STATUS OwThreadYieldExecution(void);

OW_STATUS OwThreadQueryInfo(OW_THREAD_HANDLE Handle, OW_THREAD_DESCRIPTOR *OutDesc);

#endif /* OWTHREAD64_H */
