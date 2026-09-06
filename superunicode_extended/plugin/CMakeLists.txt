cmake_minimum_required(VERSION 3.10)
project(sucs_plugin C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Include directories: this component's public headers + the ExtSUCS parent
# module headers (extsucs_types.h).
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# ============================================================================
# Static Library: libsucs_plugin
# ============================================================================
add_library(sucs_plugin_static STATIC
    src/plugin_checksum.c
    src/plugin_stage.c
    src/plugin_boot.c
    src/plugin_partition.c
)

# Freestanding bare-metal flags — zero CRT dependencies (library only).
if(MSVC)
    target_compile_options(sucs_plugin_static PRIVATE /X)
else()
    target_compile_options(sucs_plugin_static PRIVATE -ffreestanding -Wall -Wextra -O2)
endif()

set_target_properties(sucs_plugin_static PROPERTIES
    OUTPUT_NAME sucs_plugin
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# ============================================================================
# Host Tools (plugin_pack / plugin_verify)
# ============================================================================
add_executable(plugin_pack tools/plugin_pack.c src/plugin_checksum.c)
add_executable(plugin_verify tools/plugin_verify.c src/plugin_checksum.c)
set_target_properties(plugin_pack plugin_verify PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# ============================================================================
# Test Target (host CRT for printf/assert)
# ============================================================================
add_executable(test_plugin_lifecycle tests/test_plugin_lifecycle.c)
target_link_libraries(test_plugin_lifecycle sucs_plugin_static)
set_target_properties(test_plugin_lifecycle PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

enable_testing()
add_test(NAME test_plugin_lifecycle COMMAND test_plugin_lifecycle)
