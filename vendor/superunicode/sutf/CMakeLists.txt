cmake_minimum_required(VERSION 3.10)
project(sutf C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ============================================================================
# Static Library: libsutf
# ============================================================================
add_library(sutf_static STATIC
    src/sutf8.c
    src/sutf16.c
    src/sutf4.c
    src/sutf2.c
    src/sucs_mode.c
)

# Freestanding bare-metal flags — zero CRT dependencies (library only; the
# host test executable below keeps normal CRT linkage).
if(MSVC)
    target_compile_options(sutf_static PRIVATE /X)
else()
    target_compile_options(sutf_static PRIVATE -std=c99 -nostdlib -ffreestanding -Wall -Wextra -O2)
endif()

set_target_properties(sutf_static PROPERTIES
    OUTPUT_NAME sutf
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# ============================================================================
# Test Targets (linked against host CRT for printf/assert)
# ============================================================================
add_executable(test_sutf_all tests/test_sutf_all.c)
target_link_libraries(test_sutf_all sutf_static)
set_target_properties(test_sutf_all PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

enable_testing()
add_test(NAME test_sutf_all COMMAND test_sutf_all)
