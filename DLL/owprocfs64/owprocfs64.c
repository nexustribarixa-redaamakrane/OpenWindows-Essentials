/*
 * owprocfs64.c - OpenWindows Executive Process Information Query Implementation
 *
 * C99 freestanding. Zero dynamic heap allocation.
 */

#include "owprocfs64.h"

static OW_SYSTEM_PROCESS_INFORMATION g_PsInfo;
static bool g_PsInitialized = false;

static void ow_memzero(void *dst, size_t size)
{
    volatile uint8_t *d = (volatile uint8_t *)dst;
    while (size--) {
        *d++ = 0u;
    }
}

static void ow_strlcpy(char *dst, const char *src, size_t max_len)
{
    size_t i = 0;
    if (!dst || max_len == 0) return;
    if (src) {
        while (src[i] && i + 1 < max_len) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static void OwPsInitializeDefaults(void)
{
    ow_memzero(&g_PsInfo, sizeof(g_PsInfo));

    /* Seed Kernel System Process (PID 0) */
    g_PsInfo.Processes[0].ProcessId = 0;
    g_PsInfo.Processes[0].ParentProcessId = 0;
    g_PsInfo.Processes[0].SessionId = 0;
    g_PsInfo.Processes[0].UserPrincipalSid = 1; /* System */
    g_PsInfo.Processes[0].ThreadCount = 4;
    g_PsInfo.Processes[0].State = OW_PS_PROC_RUNNING;
    g_PsInfo.Processes[0].WorkingSetPages = 256;
    g_PsInfo.Processes[0].CreateTimestamp = 0;
    ow_strlcpy(g_PsInfo.Processes[0].ImageName, "openwinkrnl.owx", OW_PS_NAME_MAX);

    /* Seed Master Session Initializer (PID 1) */
    g_PsInfo.Processes[1].ProcessId = 1;
    g_PsInfo.Processes[1].ParentProcessId = 0;
    g_PsInfo.Processes[1].SessionId = 0;
    g_PsInfo.Processes[1].UserPrincipalSid = 1;
    g_PsInfo.Processes[1].ThreadCount = 1;
    g_PsInfo.Processes[1].State = OW_PS_PROC_RUNNING;
    g_PsInfo.Processes[1].WorkingSetPages = 64;
    g_PsInfo.Processes[1].CreateTimestamp = 100;
    ow_strlcpy(g_PsInfo.Processes[1].ImageName, "owinit.owx", OW_PS_NAME_MAX);

    g_PsInfo.TotalProcessCount = 2;
    g_PsInfo.ActiveThreadCount = 5;
    g_PsInfo.SystemUptimeTicks = 1000;
    g_PsInitialized = true;
}

OW_STATUS OwPsQuerySystemInformation(OW_SYSTEM_PROCESS_INFORMATION *OutInfo)
{
    uint32_t i;
    if (!OutInfo) return OW_ERR_NULL_POINTER;
    if (!g_PsInitialized) OwPsInitializeDefaults();

    OutInfo->TotalProcessCount = g_PsInfo.TotalProcessCount;
    OutInfo->ActiveThreadCount = g_PsInfo.ActiveThreadCount;
    OutInfo->SystemUptimeTicks = g_PsInfo.SystemUptimeTicks;

    for (i = 0; i < OW_PS_MAX_QUERY_PROCS; i++) {
        OutInfo->Processes[i] = g_PsInfo.Processes[i];
    }

    return OW_SUCCESS;
}

OW_STATUS OwPsQueryProcessRecord(uint32_t ProcessId, OW_PROCESS_ENTRY *OutEntry)
{
    uint32_t i;
    if (!OutEntry) return OW_ERR_NULL_POINTER;
    if (!g_PsInitialized) OwPsInitializeDefaults();

    for (i = 0; i < g_PsInfo.TotalProcessCount; i++) {
        if (g_PsInfo.Processes[i].ProcessId == ProcessId) {
            *OutEntry = g_PsInfo.Processes[i];
            return OW_SUCCESS;
        }
    }

    return OW_ERR_NOT_FOUND;
}

OW_STATUS OwPsRegisterProcessSnapshot(const OW_PROCESS_ENTRY *Entry)
{
    uint32_t i;
    if (!Entry) return OW_ERR_NULL_POINTER;
    if (!g_PsInitialized) OwPsInitializeDefaults();

    /* Update existing if found */
    for (i = 0; i < g_PsInfo.TotalProcessCount; i++) {
        if (g_PsInfo.Processes[i].ProcessId == Entry->ProcessId) {
            g_PsInfo.Processes[i] = *Entry;
            return OW_SUCCESS;
        }
    }

    if (g_PsInfo.TotalProcessCount >= OW_PS_MAX_QUERY_PROCS) {
        return OW_ERR_INSUFFICIENT;
    }

    g_PsInfo.Processes[g_PsInfo.TotalProcessCount++] = *Entry;
    g_PsInfo.ActiveThreadCount += Entry->ThreadCount;
    return OW_SUCCESS;
}

OW_STATUS OwPsUpdateProcessState(uint32_t ProcessId, OW_PROCESS_STATE NewState)
{
    uint32_t i;
    if (!g_PsInitialized) OwPsInitializeDefaults();

    for (i = 0; i < g_PsInfo.TotalProcessCount; i++) {
        if (g_PsInfo.Processes[i].ProcessId == ProcessId) {
            g_PsInfo.Processes[i].State = NewState;
            return OW_SUCCESS;
        }
    }

    return OW_ERR_NOT_FOUND;
}
