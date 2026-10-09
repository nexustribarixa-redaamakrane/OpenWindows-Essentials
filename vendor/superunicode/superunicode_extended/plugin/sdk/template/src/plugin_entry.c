/**
 * <PLUGIN-ID> — Plugin Entry Point (REQUIRED)
 *
 * The single hard requirement of a SuperUnicode plugin. Returns a
 * statically-initialized sucs_plugin_t declaring the plugin's codepoint
 * ranges (all > 0x7FFFFFFF) and optional data table.
 */

#include "superunicode_extended/plugin.h"

/* Codepoint data table from plugin_data.c (optional). */
extern const sucs_plugin_data_entry_t your_plugin_data[];

/* Declared ranges: MUST match ranges.txt and stay above the base limit. */
static const sucs_plugin_range_t g_ranges[] = {
    { 0x80000000ULL, 0x80000FFFULL },
};

static const sucs_plugin_t g_plugin = {
    .id         = "<org.openwindows.yourplugin>",
    .ver_major  = 1,
    .ver_minor  = 0,
    .ver_patch  = 0,
    .base_limit = SUCS_PLUGIN_BASE_LIMIT,   /* 0x7FFFFFFF */
    .range_count = 1,
    .ranges     = g_ranges,
    .data       = your_plugin_data,
    .data_count = 1,
};

const sucs_plugin_t* sucs_plugin_entry(void) {
    return &g_plugin;
}
