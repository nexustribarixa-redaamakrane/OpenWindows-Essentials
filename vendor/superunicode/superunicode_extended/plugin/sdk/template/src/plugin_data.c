/**
 * <PLUGIN-ID> — Codepoint Data Table (optional)
 *
 * Each entry maps one plugin-owned codepoint to a name and type:
 *   type 0 = printable native allocation (advances the cursor)
 *   type 1 = control / machine instruction (does not advance the cursor)
 */

#include "superunicode_extended/plugin.h"

const sucs_plugin_data_entry_t your_plugin_data[] = {
    { 0x80000000ULL, "YOURPLUGIN.CODEPOINT_ONE",   0 },
    { 0x80000001ULL, "YOURPLUGIN.CODEPOINT_TWO",   0 },
};
