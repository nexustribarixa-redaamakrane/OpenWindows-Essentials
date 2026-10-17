/*
 * owprocfs64.h - OpenWindows Executive Process Information Query Library (.owd)
 *
 * Replaces POSIX /proc synthetic filesystem with OpenWindows NT-like
 * Executive Process & Thread Information queries conforming to ow_ps.h.
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#ifndef OWPROCFS64_H
#define OWPROCFS64_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define OW_SUCCESS              0x00000000U
#define OW_ERR_INVALID_PARAM    0x0011AEE0U
#define OW_ERR_NULL_POINTER     0x0011AEE1U
#define OW_ERR_NOT_FOUND        0x0011AEE2U
#define OW_ERR_INSUFFICIENT     0x0011AEE4U

typedef uint32_t OW_STATUS;

#define OW_PS_MAX_QUERY_PROCS   32U
#define OW_PS_NAME_MAX          32U

typedef enum _OW_PROCESS_STATE {
    OW_PS_PROC_CREATED = 0,
    OW_PS_PROC_RUNNING = 1,
    OW_PS_PROC_TERMINATED = 2
} OW_PROCESS_STATE;

typedef enum _OW_THREAD_STATE {
    OW_THR_DORMANT = 0,
    OW_THR_READY = 1,
    OW_THR_RUNNING = 2,
    OW_THR_BLOCKED = 3,
    OW_THR_TERMINATED = 4
} OW_THREAD_STATE;

typedef struct _OW_PROCESS_ENTRY {
    uint32_t            ProcessId;
    uint32_t            ParentProcessId;
    uint32_t            SessionId;
    uint32_t            UserPrincipalSid;
    uint32_t            ThreadCount;
    OW_PROCESS_STATE    State;
    uint32_t            WorkingSetPages;
    uint64_t            CreateTimestamp;
    uint64_t            KernelCpuTicks;
    uint64_t            UserCpuTicks;
    char                ImageName[OW_PS_NAME_MAX];
} OW_PROCESS_ENTRY;

typedef struct _OW_SYSTEM_PROCESS_INFORMATION {
    uint32_t            TotalProcessCount;
    uint32_t            ActiveThreadCount;
    uint64_t            SystemUptimeTicks;
    OW_PROCESS_ENTRY    Processes[OW_PS_MAX_QUERY_PROCS];
} OW_SYSTEM_PROCESS_INFORMATION;

/* ── Public API ───────────────────────────────────────────────────── */

OW_STATUS OwPsQuerySystemInformation(OW_SYSTEM_PROCESS_INFORMATION *OutInfo);

OW_STATUS OwPsQueryProcessRecord(uint32_t ProcessId, OW_PROCESS_ENTRY *OutEntry);

OW_STATUS OwPsRegisterProcessSnapshot(const OW_PROCESS_ENTRY *Entry);

OW_STATUS OwPsUpdateProcessState(uint32_t ProcessId, OW_PROCESS_STATE NewState);

#endif /* OWPROCFS64_H */
