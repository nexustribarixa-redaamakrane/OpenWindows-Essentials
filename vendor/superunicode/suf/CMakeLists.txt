cmake_minimum_required(VERSION 3.10)
project(suf C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../superunicode/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../sutf/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../superunicode_extended/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../superunicode_extended/plugin/include
)

# Core freestanding parser & builder library
add_library(suf_static STATIC
    src/suf_parser.c
    src/suf_builder.c
    src/suf_conv.c
)
target_include_directories(suf_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add tools subdirectory
add_subdirectory(tools)

# Test suite
add_executable(test_suf tests/test_suf.c)
target_link_libraries(test_suf suf_static)

enable_testing()
add_test(NAME test_suf COMMAND test_suf)

