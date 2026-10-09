cmake_minimum_required(VERSION 3.10)
project(sust C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../sutf/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../superunicode_extended/include)

# ============================================================================
# Static Library: libsust
# ============================================================================
add_library(sust_static STATIC
    src/sust16.c
    src/sustfixed.c
    src/esust.c
)

# Freestanding bare-metal flags — zero CRT dependencies (library only; the
# host test executable below keeps normal CRT linkage).
if(MSVC)
    target_compile_options(sust_static PRIVATE /X)
else()
    target_compile_options(sust_static PRIVATE -std=c99 -nostdlib -ffreestanding -Wall -Wextra -O2)
endif()

set_target_properties(sust_static PROPERTIES
    OUTPUT_NAME sust
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# ============================================================================
# Test Targets (linked against host CRT for printf/assert)
# ============================================================================
add_executable(test_sust_all tests/test_sust_all.c)
target_link_libraries(test_sust_all sust_static)
set_target_properties(test_sust_all PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

enable_testing()
add_test(NAME test_sust_all COMMAND test_sust_all)