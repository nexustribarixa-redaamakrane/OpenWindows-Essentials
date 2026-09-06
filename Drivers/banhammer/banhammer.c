/*
 * banhammer.c - OpenWindows Banhammer Driver Implementation
 *
 * Core panic handler: validates config, captures CPU state,
 * writes telemetry to ring buffer, and executes emergency
 * halt/reboot on x86_64.
 *
 * C99 freestanding. Zero heap. x86_64 specific for halt/reboot.
 */

#include "banhammer.h"

/* ------------------------------------------------------------------ */
/*  Portable Inline Assembly                                           */
/* ------------------------------------------------------------------ */

#if defined(__GNUC__) || defined(__clang__)
#define BH_ASM          __asm__
#define BH_ASM_VOL      __asm__ volatile
#elif defined(_MSC_VER)
#define BH_ASM          __asm
#define BH_ASM_VOL      __asm
#else
#define BH_ASM          /* no inline asm */
#define BH_ASM_VOL      /* no inline asm */
#endif

/* ------------------------------------------------------------------ */
/*  Internal Helpers                                                    */
/* ------------------------------------------------------------------ */

static void banhammer_memset(void *dst, uint8_t val, size_t size)
{
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < size; i++)
        d[i] = val;
}

static size_t banhammer_str_copy(char *dst, const char *src, size_t max)
{
    size_t i = 0;
    if (max == 0)
        return 0;

    while (src[i] != '\0' && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return i;
}

static size_t banhammer_str_len(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

/*
 * CRC-32c (iSCSI polynomial 0x82F63B78 reflected).
 * Bitwise computation, no lookup table. Matches damage_control.c.
 */
static uint32_t banhammer_crc32c_byte(uint32_t crc, uint8_t byte)
{
    crc ^= (uint32_t)byte;
    for (int j = 0; j < 8; j++) {
        if (crc & 1u)
            crc = (crc >> 1) ^ 0x82F63B78u;
        else
            crc >>= 1;
    }
    return crc;
}

static uint32_t banhammer_crc32c(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFu;

    for (uint32_t i = 0; i < length; i++)
        crc = banhammer_crc32c_byte(crc, data[i]);

    return crc ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/*  banhammer_init                                                             */
/* ------------------------------------------------------------------ */

banhammer_status_t banhammer_init(banhammer_context_t *ctx, bhcf_config_t *config)
{
    if (ctx == NULL || config == NULL)
        return BANHAMMER_ERR_INVALID_CONFIG;

    if (bhcf_validate(config) != BHCF_OK)
        return BANHAMMER_ERR_INVALID_CONFIG;

    volatile uint8_t *dst = (volatile uint8_t *)ctx;
    for (size_t i = 0; i < sizeof(banhammer_context_t); i++)
        dst[i] = 0;

    ctx->config   = config;
    ctx->initialized = true;
    ctx->ring_head  = 0;
    ctx->ring_count = 0;
    ctx->string_used = 0;

    return BANHAMMER_OK;
}

/* ------------------------------------------------------------------ */
/*  banhammer_release                                                          */
/* ------------------------------------------------------------------ */

void banhammer_release(banhammer_context_t *ctx)
{
    if (ctx == NULL)
        return;

    volatile uint8_t *dst = (volatile uint8_t *)ctx;
    for (size_t i = 0; i < sizeof(banhammer_context_t); i++)
        dst[i] = 0;

    ctx->initialized = false;
}

/* ------------------------------------------------------------------ */
/*  banhammer_capture_cpu                                                      */
/* ------------------------------------------------------------------ */

banhammer_status_t banhammer_capture_cpu(
    banhammer_context_t *ctx,
    uint64_t rax, uint64_t rbx, uint64_t rcx, uint64_t rdx,
    uint64_t rsi, uint64_t rdi, uint64_t rbp, uint64_t rsp,
    uint64_t r8,  uint64_t r9,  uint64_t r10, uint64_t r11,
    uint64_t r12, uint64_t r13, uint64_t r14, uint64_t r15,
    uint64_t rip, uint64_t rflags,
    uint64_t cr0, uint64_t cr2, uint64_t cr3, uint64_t cr4,
    uint16_t cs,  uint16_t ds,  uint16_t es,  uint16_t fs,
    uint16_t gs,  uint16_t ss
)
{
    if (ctx == NULL)
        return BANHAMMER_ERR_CPU_CAPTURE_FAILED;

    ctx->cpu_state.rax   = rax;
    ctx->cpu_state.rbx   = rbx;
    ctx->cpu_state.rcx   = rcx;
    ctx->cpu_state.rdx   = rdx;
    ctx->cpu_state.rsi   = rsi;
    ctx->cpu_state.rdi   = rdi;
    ctx->cpu_state.rbp   = rbp;
    ctx->cpu_state.rsp   = rsp;
    ctx->cpu_state.r8    = r8;
    ctx->cpu_state.r9    = r9;
    ctx->cpu_state.r10   = r10;
    ctx->cpu_state.r11   = r11;
    ctx->cpu_state.r12   = r12;
    ctx->cpu_state.r13   = r13;
    ctx->cpu_state.r14   = r14;
    ctx->cpu_state.r15   = r15;
    ctx->cpu_state.rip   = rip;
    ctx->cpu_state.rflags = rflags;
    ctx->cpu_state.cr0   = cr0;
    ctx->cpu_state.cr2   = cr2;
    ctx->cpu_state.cr3   = cr3;
    ctx->cpu_state.cr4   = cr4;
    ctx->cpu_state.cs    = cs;
    ctx->cpu_state.ds    = ds;
    ctx->cpu_state.es    = es;
    ctx->cpu_state.fs    = fs;
    ctx->cpu_state.gs    = gs;
    ctx->cpu_state.ss    = ss;

    /* Read TSC */
    uint32_t lo, hi;
    BH_ASM_VOL("rdtsc" : "=a"(lo), "=d"(hi));
    ctx->cpu_state.tsc = ((uint64_t)hi << 32) | lo;

    return BANHAMMER_OK;
}

/* ------------------------------------------------------------------ */
/*  banhammer_bancode_to_trap_slot                                             */
/* ------------------------------------------------------------------ */

uint8_t banhammer_bancode_to_trap_slot(bancode_t bancode)
{
    if (bancode < BH_BANCODE_START || bancode > BH_BANCODE_END)
        return 0xFF;

    uint32_t slot = (bancode - BH_BANCODE_START) / BH_BANCODES_PER_TRAP;

    if (slot >= BH_TRAP_SLOT_COUNT)
        return 0xFF;

    return (uint8_t)slot;
}

/* ------------------------------------------------------------------ */
/*  banhammer_bancode_name                                                     */
/* ------------------------------------------------------------------ */

const char *banhammer_bancode_name(bancode_t bancode)
{
    switch (bancode) {
    case 0x0011A2E0u: return "MBL_BAN_SB_MAGIC";
    case 0x0011A2E1u: return "MBL_BAN_SB_CHECKSUM";
    case 0x0011A2E2u: return "MBL_BAN_INODE_CHECKSUM";
    case 0x0011A2E3u: return "MBL_BAN_CATALOG_CHECKSUM";
    case 0x0011A2E4u: return "MBL_BAN_IO_ERROR";
    case 0x0011A2E5u: return "MBL_BAN_LOAD_FAILED";
    default:          return "UNKNOWN_BANCODE";
    }
}

/* ------------------------------------------------------------------ */
/*  banhammer_telemetry_write                                                  */
/* ------------------------------------------------------------------ */

banhammer_status_t banhammer_telemetry_write(
    banhammer_context_t *ctx,
    bancode_t     bancode,
    uint8_t       severity,
    const char   *message,
    uint64_t      timestamp
)
{
    if (ctx == NULL || ctx->config == NULL)
        return BANHAMMER_ERR_NO_CONFIG;

    if (ctx->ring_count >= BH_TELEMETRY_RING_SIZE)
        return BANHAMMER_ERR_TELEMETRY_FULL;

    uint32_t index = (ctx->ring_head + ctx->ring_count) % BH_TELEMETRY_RING_SIZE;
    banhammer_telemetry_record_t *rec = &ctx->ring[index];

    banhammer_memset(rec, 0, sizeof(banhammer_telemetry_record_t));

    rec->timestamp      = timestamp;
    rec->bancode        = bancode;
    rec->severity       = severity;
    rec->behavior_flags = ctx->config->behavior_flags;
    rec->cpu_rip        = (uint32_t)(ctx->cpu_state.rip & 0xFFFFFFFFu);
    rec->cpu_cr2        = (uint32_t)(ctx->cpu_state.cr2 & 0xFFFFFFFFu);
    rec->trap_slot      = banhammer_bancode_to_trap_slot(bancode);

    /* Copy message into string pool */
    if (message != NULL && ctx->string_used < BH_TELEMETRY_RING_SIZE * BH_TELEMETRY_MSG_MAX) {
        size_t avail = (BH_TELEMETRY_RING_SIZE * BH_TELEMETRY_MSG_MAX) - ctx->string_used;
        size_t copied = banhammer_str_copy(&ctx->string_pool[ctx->string_used], message, avail);
        rec->message_offset = ctx->string_used;
        ctx->string_used += (uint32_t)copied + 1;
    } else {
        rec->message_offset = 0;
    }

    ctx->ring_count++;
    return BANHAMMER_OK;
}

/* ------------------------------------------------------------------ */
/*  banhammer_telemetry_flush                                                  */
/* ------------------------------------------------------------------ */

banhammer_status_t banhammer_telemetry_flush(banhammer_context_t *ctx)
{
    if (ctx == NULL || ctx->config == NULL)
        return BANHAMMER_ERR_TELEMETRY_WRITE;

    if (ctx->config->telemetry_lba == 0)
        return BANHAMMER_ERR_TELEMETRY_WRITE;

    /*
     * Freestanding: no actual block I/O available.
     * In a real system this writes a telemetry block header:
     *   Magic:   0x4254454C ("BTEL")
     *   Count:   ctx->ring_count
     *   CRC32c:  of the entire ring data
     * via htl_write_block(ctx->config->telemetry_lba, ...).
     *
     * Validate config has valid telemetry_lba; if so, success.
     */

    ctx->total_telemetry_writes++;
    return BANHAMMER_OK;
}

/* ------------------------------------------------------------------ */
/*  banhammer_telemetry_last                                                   */
/* ------------------------------------------------------------------ */

const banhammer_telemetry_record_t *banhammer_telemetry_last(const banhammer_context_t *ctx)
{
    if (ctx == NULL || ctx->ring_count == 0)
        return NULL;

    uint32_t index = (ctx->ring_head + ctx->ring_count - 1) % BH_TELEMETRY_RING_SIZE;
    return &ctx->ring[index];
}

/* ------------------------------------------------------------------ */
/*  banhammer_telemetry_count                                                  */
/* ------------------------------------------------------------------ */

uint32_t banhammer_telemetry_count(const banhammer_context_t *ctx)
{
    if (ctx == NULL)
        return 0;
    return ctx->ring_count;
}

/* ------------------------------------------------------------------ */
/*  banhammer_emergency_halt  (x86_64, does not return)                       */
/* ------------------------------------------------------------------ */

void banhammer_emergency_halt(void)
{
    BH_ASM_VOL("cli");
    for (;;) {
        BH_ASM_VOL("hlt");
    }
}

/* ------------------------------------------------------------------ */
/*  banhammer_emergency_reboot  (x86_64, does not return)                     */
/* ------------------------------------------------------------------ */

void banhammer_emergency_reboot(void)
{
    BH_ASM_VOL("cli");

    /* Try keyboard controller reset (port 0x64, cmd 0xFE) */
    BH_ASM_VOL("outb %0, %1" : : "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));

    /* Fallback: triple fault via invalid IDT limit */
    struct {
        uint16_t limit;
        uint64_t base;
    } BH_PACKED idt = { 0, 0 };
    BH_ASM_VOL("lidt %0" : : "m"(idt));
    BH_ASM_VOL("int $3");

    for (;;) {
        BH_ASM_VOL("hlt");
    }
}

/* ------------------------------------------------------------------ */
/*  banhammer_memory_dump                                                      */
/* ------------------------------------------------------------------ */

banhammer_status_t banhammer_memory_dump(banhammer_context_t *ctx)
{
    if (ctx == NULL || ctx->config == NULL)
        return BANHAMMER_ERR_DUMP_FAILED;

    if (ctx->config->dump_region_count == 0)
        return BANHAMMER_ERR_DUMP_FAILED;

    if (ctx->config->dump_region_count > BHCF_MAX_REGIONS)
        return BANHAMMER_ERR_DUMP_FAILED;

    uint32_t total = 0;
    for (uint8_t i = 0; i < ctx->config->dump_region_count; i++) {
        /* Dump region descriptors follow the config header at offset BHCF_HEADER_SIZE */
        const bhcf_dump_region_t *region =
            (const bhcf_dump_region_t *)((const uint8_t *)ctx->config + BHCF_HEADER_SIZE + (i * sizeof(bhcf_dump_region_t)));

        if (region->size_bytes == 0)
            return BANHAMMER_ERR_DUMP_FAILED;

        total += region->size_bytes;
    }

    if (total > ctx->config->max_dump_size)
        return BANHAMMER_ERR_DUMP_FAILED;

    /*
     * Freestanding: no actual I/O. In a real system, reads memory
     * at each region's base_address and writes to storage at
     * telemetry_lba + offset.
     * Regions validated; return success.
     */

    return BANHAMMER_OK;
}

/* ------------------------------------------------------------------ */
/*  banhammer_strike  (main panic handler)                                    */
/* ------------------------------------------------------------------ */

banhammer_status_t banhammer_strike(
    banhammer_context_t *ctx,
    bancode_t    bancode,
    uint8_t      severity,
    const char  *message
)
{
    /* 1. Validate context and config */
    if (ctx == NULL || ctx->config == NULL)
        return BANHAMMER_ERR_NO_CONFIG;

    if (!ctx->initialized)
        return BANHAMMER_ERR_NO_CONFIG;

    if (bhcf_validate(ctx->config) != BHCF_OK)
        return BANHAMMER_ERR_NO_CONFIG;

    /* 2. Validate severity range and threshold */
    if (severity > BHCF_SEVERITY_CATASTROPHIC)
        return BANHAMMER_ERR_SEVERITY_LOW;

    if (severity != BHCF_SEVERITY_CATASTROPHIC) {
        if (!bhcf_severity_triggers_halt(ctx->config, severity))
            return BANHAMMER_ERR_SEVERITY_LOW;
    }

    /* 3. Increment counters */
    ctx->total_panics++;
    ctx->total_strikes++;

    /* 4. Record last event */
    ctx->last_severity = severity;
    ctx->last_bancode  = bancode;

    uint32_t tsc_lo, tsc_hi;
    BH_ASM_VOL("rdtsc" : "=a"(tsc_lo), "=d"(tsc_hi));
    ctx->last_timestamp = ((uint64_t)tsc_hi << 32) | tsc_lo;

    /* 5. Write telemetry record */
    banhammer_telemetry_write(ctx, bancode, severity, message, ctx->last_timestamp);

    /* 6. Execute behavior based on flags */
    uint8_t flags = ctx->config->behavior_flags;

    if (flags & BHCF_FLAG_TELEMETRY_ON) {
        banhammer_telemetry_flush(ctx);
    }

    if (flags & BHCF_FLAG_MEMORY_DUMP) {
        banhammer_memory_dump(ctx);
        ctx->total_dumps++;
    }

    if ((flags & BHCF_FLAG_AUTO_REBOOT) && severity >= BHCF_SEVERITY_CRITICAL) {
        ctx->total_reboots++;

        /* Busy-loop delay based on reboot_delay_ms using TSC */
        if (ctx->config->reboot_delay_ms > 0) {
            /* Assume ~1 GHz TSC for approximation: 1 ms ~ 1,000,000 cycles */
            uint64_t delay_tsc = (uint64_t)ctx->config->reboot_delay_ms * 1000000ULL;
            uint32_t d_lo, d_hi;
            BH_ASM_VOL("rdtsc" : "=a"(d_lo), "=d"(d_hi));
            uint64_t start = ((uint64_t)d_hi << 32) | d_lo;

            for (;;) {
                BH_ASM_VOL("rdtsc" : "=a"(d_lo), "=d"(d_hi));
                uint64_t now = ((uint64_t)d_hi << 32) | d_lo;
                if ((now - start) >= delay_tsc)
                    break;
            }
        }

        banhammer_emergency_reboot();
    }

    /* banhammer_emergency_halt does not return, so reaching here means
     * severity was below threshold or no halt flag was set. */
    return BANHAMMER_OK;
}

/* ------------------------------------------------------------------ */
/*  banhammer_get_stats                                                        */
/* ------------------------------------------------------------------ */

void banhammer_get_stats(
    const banhammer_context_t *ctx,
    uint32_t *total_panics,
    uint32_t *total_strikes,
    uint32_t *total_reboots,
    uint32_t *total_dumps
)
{
    if (ctx == NULL)
        return;

    if (total_panics != NULL)
        *total_panics = ctx->total_panics;
    if (total_strikes != NULL)
        *total_strikes = ctx->total_strikes;
    if (total_reboots != NULL)
        *total_reboots = ctx->total_reboots;
    if (total_dumps != NULL)
        *total_dumps = ctx->total_dumps;
}
