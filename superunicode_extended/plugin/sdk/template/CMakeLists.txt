cmake_minimum_required(VERSION 3.10)
project(your_plugin C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Point these at your checkout of the SuperUnicode workspace.
include_directories(
    ${SUCS_ROOT}/superunicode_extended/plugin/include
    ${SUCS_ROOT}/superunicode_extended/include
)

# Your plugin sources compile against the loader ABI (plugin.h).
add_library(your_plugin STATIC
    src/plugin_entry.c
    src/plugin_data.c
    src/plugin_mappings.c
    src/plugin_collation.c
)

# Pack step (host tool from the plugin component):
#   plugin_pack <id> <major> <minor> <patch> ranges.txt payload.bin <id>.sucsplugin
