/**
 * hellocp — Hello Codepoints plugin
 *
 * A minimal, fully packable example plugin. Declares 4096 codepoints at
 * 0x80000000-0x80000FFF (above the base limit) and a two-entry data table.
 */

#include "superunicode_extended/plugin.h"

extern const sucs_plugin_data_entry_t hellocp_data[];

static const sucs_plugin_range_t g_ranges[] = {
    { 0x80000000ULL, 0x80000FFFULL },
};

static const sucs_plugin_t g_plugin = {
    .id         = "org.openwindows.hellocp",
    .ver_major  = 1,
    .ver_minor  = 0,
    .ver_patch  = 0,
    .base_limit = SUCS_PLUGIN_BASE_LIMIT,
    .range_count = 1,
    .ranges     = g_ranges,
    .data       = hellocp_data,
    .data_count = 2,
};

const sucs_plugin_t* sucs_plugin_entry(void) {
    return &g_plugin;
}
