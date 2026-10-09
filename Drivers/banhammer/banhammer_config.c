/*
 * banhammer_config.c - BHCF Configuration Parser Implementation
 *
 * C99 freestanding. Zero heap allocation.
 */

#include "banhammer_config.h"

/* ------------------------------------------------------------------ */
/*  CRC32c (Castagnoli)                                                */
/* ------------------------------------------------------------------ */

static uint32_t crc32c_byte(uint32_t crc, uint8_t byte) {
    crc ^= (uint32_t)byte;
    for (int i = 0; i < 8; i++) {
        if (crc & 1u)
            crc = (crc >> 1) ^ 0x82F63B78u;
        else
            crc = crc >> 1;
    }
    return crc;
}

static uint32_t crc32c(const void *data, uint32_t size) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < size; i++)
        crc = crc32c_byte(crc, p[i]);
    return crc ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/*  Checksum Recomputation                                             */
/* ------------------------------------------------------------------ */

void bhcf_recompute_checksums(bhcf_config_t *cfg) {
    uint8_t *raw = (uint8_t *)cfg;
    uint32_t saved_cfg_crc;
    uint32_t saved_img_crc;

    /* config_checksum: CRC32c over bytes 0x08-0x3F, field zeroed */
    saved_cfg_crc = cfg->config_checksum;
    cfg->config_checksum = 0;
    cfg->config_checksum = crc32c(raw + 0x08, 0x38);

    /* image_checksum: CRC32c over full 64-byte block, field zeroed */
    saved_img_crc = cfg->image_checksum;
    cfg->image_checksum = 0;
    cfg->image_checksum = crc32c(raw, BHCF_HEADER_SIZE);

    (void)saved_cfg_crc;
    (void)saved_img_crc;
}

/* ------------------------------------------------------------------ */
/*  Validation                                                         */
/* ------------------------------------------------------------------ */

bhcf_status_t bhcf_validate(const bhcf_config_t *cfg) {
    uint32_t computed;
    uint32_t saved;
    const uint8_t *raw;

    if (!cfg)
        return BHCF_ERR_NULL_POINTER;

    if (cfg->magic != BHCF_MAGIC)
        return BHCF_ERR_INVALID_MAGIC;

    if (cfg->format_version != BHCF_FORMAT_VERSION)
        return BHCF_ERR_UNSUPPORTED_VER;

    if (cfg->header_size != BHCF_HEADER_SIZE)
        return BHCF_ERR_HEADER_CRC;

    raw = (const uint8_t *)cfg;

    /* Verify config_checksum (bytes 0x08-0x3F, field zeroed) */
    saved = cfg->config_checksum;
    ((bhcf_config_t *)cfg)->config_checksum = 0;
    computed = crc32c(raw + 0x08, 0x38);
    ((bhcf_config_t *)cfg)->config_checksum = saved;
    if (computed != saved)
        return BHCF_ERR_CONFIG_CRC;

    /* Verify image_checksum (full block, field zeroed) */
    saved = cfg->image_checksum;
    ((bhcf_config_t *)cfg)->image_checksum = 0;
    computed = crc32c(raw, BHCF_HEADER_SIZE);
    ((bhcf_config_t *)cfg)->image_checksum = saved;
    if (computed != saved)
        return BHCF_ERR_HEADER_CRC;

    return BHCF_OK;
}

/* ------------------------------------------------------------------ */
/*  Initialization                                                     */
/* ------------------------------------------------------------------ */

bhcf_status_t bhcf_init_defaults(bhcf_config_t *cfg) {
    if (!cfg)
        return BHCF_ERR_NULL_POINTER;

    uint8_t *raw = (uint8_t *)cfg;
    for (uint32_t i = 0; i < sizeof(bhcf_config_t); i++)
        raw[i] = 0;

    cfg->magic              = BHCF_MAGIC;
    cfg->format_version     = BHCF_FORMAT_VERSION;
    cfg->header_size        = BHCF_HEADER_SIZE;
    cfg->behavior_flags     = BHCF_FLAG_BOTH;
    cfg->severity_threshold = BHCF_SEVERITY_CRITICAL;
    cfg->reboot_delay_ms    = 1000;
    cfg->max_dump_size      = 0x100000;
    cfg->dump_region_count  = 0;

    bhcf_recompute_checksums(cfg);
    return BHCF_OK;
}

/* ------------------------------------------------------------------ */
/*  Flag Queries & Mutations                                           */
/* ------------------------------------------------------------------ */

bool bhcf_flag_set(const bhcf_config_t *cfg, uint8_t flag) {
    if (!cfg)
        return false;
    return (cfg->behavior_flags & flag) == flag;
}

bhcf_status_t bhcf_set_flag(bhcf_config_t *cfg, uint8_t flag, bool enable) {
    if (!cfg)
        return BHCF_ERR_NULL_POINTER;

    if (enable)
        cfg->behavior_flags |= flag;
    else
        cfg->behavior_flags &= (uint8_t)~flag;

    bhcf_recompute_checksums(cfg);

    if (bhcf_validate(cfg) != BHCF_OK)
        return BHCF_ERR_INVALID_FLAGS;

    return BHCF_OK;
}

/* ------------------------------------------------------------------ */
/*  Severity Evaluation                                                */
/* ------------------------------------------------------------------ */

bool bhcf_severity_triggers_halt(const bhcf_config_t *cfg, uint8_t severity) {
    if (!cfg)
        return false;
    if (severity >= BHCF_SEVERITY_CATASTROPHIC)
        return true;
    return severity >= cfg->severity_threshold;
}

/* ------------------------------------------------------------------ */
/*  Name Lookups                                                       */
/* ------------------------------------------------------------------ */

const char *bhcf_flag_name(uint8_t flag) {
    switch (flag) {
    case BHCF_FLAG_NONE:          return "NONE";
    case BHCF_FLAG_AUTO_REBOOT:   return "AUTO_REBOOT";
    case BHCF_FLAG_MEMORY_DUMP:   return "MEMORY_DUMP";
    case BHCF_FLAG_BOTH:          return "BOTH";
    case BHCF_FLAG_TELEMETRY_ON:  return "TELEMETRY_ON";
    case BHCF_FLAG_SERIAL_LOG:    return "SERIAL_LOG";
    case BHCF_FLAG_SENTINEL_HOOK: return "SENTINEL_HOOK";
    case BHCF_FLAG_SCREEN_PANIC:   return "SCREEN_PANIC";
    default:                      return "UNKNOWN";
    }
}

const char *bhcf_severity_name(uint8_t severity) {
    switch (severity) {
    case BHCF_SEVERITY_INFO:         return "INFO";
    case BHCF_SEVERITY_WARNING:      return "WARNING";
    case BHCF_SEVERITY_RECOVERABLE:  return "RECOVERABLE";
    case BHCF_SEVERITY_CRITICAL:     return "CRITICAL";
    case BHCF_SEVERITY_CATASTROPHIC: return "CATASTROPHIC";
    default:                         return "UNKNOWN";
    }
}
