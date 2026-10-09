cmake_minimum_required(VERSION 3.10)
project(unified_headers C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Every public include root, so a single translation unit can pull in ALL
# headers from all three modules at once (this is the point of the test).
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../superunicode/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../sutf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../superunicode_extended/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../superunicode_extended/plugin/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../sust/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../suf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

add_executable(test_unified test_unified.c)
target_link_libraries(test_unified
    superunicode_static
    sutf_static
    superunicode_extended_static
    sust_static
    sucs_plugin_static
    suf_static
    suas_static
    suts_static
)
set_target_properties(test_unified PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

enable_testing()
add_test(NAME test_unified COMMAND test_unified)
