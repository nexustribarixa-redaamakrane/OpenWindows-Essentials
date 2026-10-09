/*
 * kernel64.c - OpenWindows Base API (.owd)
 *
 * C99 freestanding only (stdint.h, stdbool.h, stddef.h).
 * Zero heap. All kernel calls go through OWRP gates.
 */

#include "kernel64.h"
#include "owrp.h"

/* ------------------------------------------------------------------ */
/*  Portable inline asm macros                                          */
/* ------------------------------------------------------------------ */

#if defined(__GNUC__) || defined(__clang__)
#define K64_ASM_VOL     __asm__ volatile
#elif defined(_MSC_VER)
#define K64_ASM_VOL     __asm
#else
#define K64_ASM_VOL     /* unsupported */
#endif

/* ------------------------------------------------------------------ */
/*  Internal state                                                     */
/* ------------------------------------------------------------------ */

static owrp_context_t k64_owrp_ctx;
static bool k64_initialized = false;
static uint64_t k64_tsc_freq_ticks_per_ms = 1000000u;

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                   */
/* ------------------------------------------------------------------ */

static uint64_t k64_read_tsc(void)
{
    uint32_t lo, hi;
    K64_ASM_VOL("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
}

static void k64_memset(void *dst, int val, size_t size)
{
    uint8_t *p = (uint8_t *)dst;
    uint8_t v = (uint8_t)val;
    for (size_t i = 0; i < size; i++)
        p[i] = v;
}

/* ------------------------------------------------------------------ */
/*  API Implementation                                                 */
/* ------------------------------------------------------------------ */

k64_status_t k64_initialize_api(void)
{
    k64_memset(&k64_owrp_ctx, 0, sizeof(k64_owrp_ctx));

    if (owrp_init(&k64_owrp_ctx) != OWRP_OK)
        return K64_ERR_GATE_FAILED;

    owrp_register_gate(&k64_owrp_ctx, OWRP_SYS_MEM_REQUEST,
        "mem_request", 2, 0, OWRP_GATE_FLAG_KERNEL);

    owrp_register_gate(&k64_owrp_ctx, OWRP_SYS_MEM_RELEASE,
        "mem_release", 1, 0, OWRP_GATE_FLAG_KERNEL);

    owrp_register_gate(&k64_owrp_ctx, OWRP_SYS_GET_TSC,
        "get_tsc", 0, 3, 0);

    owrp_register_gate(&k64_owrp_ctx, OWRP_SYS_BAN_RAISE,
        "ban_raise", 2, 0, OWRP_GATE_FLAG_KERNEL);

    owrp_register_gate(&k64_owrp_ctx, OWRP_SYS_TIMER_READ,
        "timer_read", 0, 3, 0);

    /* TSC frequency calibration via spin-wait loop */
    volatile uint64_t dummy = 0;
    uint64_t tsc_start = k64_read_tsc();
    for (volatile uint32_t i = 0; i < 10000u; i++)
        dummy += (uint64_t)i;
    uint64_t tsc_end = k64_read_tsc();

    uint64_t elapsed = tsc_end - tsc_start;
    if (elapsed > 0u)
        k64_tsc_freq_ticks_per_ms = elapsed;

    (void)dummy;

    k64_initialized = true;
    return K64_OK;
}

k64_status_t k64_query_system_time(uint64_t *out_timestamp)
{
    if (out_timestamp == NULL)
        return K64_ERR_INVALID_PARAM;
    if (!k64_initialized)
        return K64_ERR_NOT_INITIALIZED;

    owrp_gate_result_t result = owrp_invoke_gate(
        &k64_owrp_ctx, OWRP_SYS_GET_TSC, 0, 0, 0);

    if (result.error != 0)
        return K64_ERR_GATE_FAILED;

    *out_timestamp = (uint64_t)result.value;
    return K64_OK;
}

k64_status_t k64_request_memory_block(size_t size_bytes, void **out_ptr)
{
    if (!k64_initialized)
        return K64_ERR_NOT_INITIALIZED;
    if (size_bytes == 0 || out_ptr == NULL)
        return K64_ERR_INVALID_PARAM;

    owrp_gate_result_t result = owrp_invoke_gate(
        &k64_owrp_ctx, OWRP_SYS_MEM_REQUEST,
        (uint64_t)(size_bytes + K64_GUARD_PAGE_SIZE), 0, 0);

    if (result.error != 0)
        return K64_ERR_OUT_OF_MEMORY;

    *out_ptr = (void *)result.value;
    return K64_OK;
}

k64_status_t k64_release_memory_block(void *ptr)
{
    if (!k64_initialized)
        return K64_ERR_NOT_INITIALIZED;
    if (ptr == NULL)
        return K64_ERR_INVALID_POINTER;

    owrp_gate_result_t result = owrp_invoke_gate(
        &k64_owrp_ctx, OWRP_SYS_MEM_RELEASE, (uint64_t)ptr, 0, 0);

    if (result.error != 0)
        return K64_ERR_INVALID_POINTER;

    return K64_OK;
}

k64_status_t k64_raise_system_exception(uint32_t bancode, uint64_t subcode)
{
    if (!k64_initialized)
        return K64_ERR_NOT_INITIALIZED;
    if (bancode < K64_BANCODE_START || bancode > K64_BANCODE_END)
        return K64_ERR_INVALID_PARAM;

    owrp_gate_result_t result = owrp_invoke_gate(
        &k64_owrp_ctx, OWRP_SYS_BAN_RAISE,
        (uint64_t)bancode, subcode, 0);

    if (result.error != 0)
        return K64_ERR_GATE_FAILED;

    return K64_ERR_BANCODE_RAISED;
}

k64_status_t k64_get_system_time_full(k64_system_time_t *out_time)
{
    if (out_time == NULL)
        return K64_ERR_INVALID_PARAM;
    if (!k64_initialized)
        return K64_ERR_NOT_INITIALIZED;

    uint64_t tsc = k64_read_tsc();

    out_time->tsc = tsc;
    out_time->ticks_per_ms = k64_tsc_freq_ticks_per_ms;

    uint64_t total_ms = tsc / k64_tsc_freq_ticks_per_ms;
    out_time->seconds = (uint32_t)(total_ms / 1000u);
    out_time->milliseconds = (uint32_t)(total_ms % 1000u);

    return K64_OK;
}

bool k64_api_ready(void)
{
    return k64_initialized;
}

void k64_get_version(uint32_t *major, uint32_t *minor, uint32_t *patch)
{
    if (major != NULL)
        *major = K64_VERSION_MAJOR;
    if (minor != NULL)
        *minor = K64_VERSION_MINOR;
    if (patch != NULL)
        *patch = K64_VERSION_PATCH;
}
