cmake_minimum_required(VERSION 3.10)
project(superunicode C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Public Include Directory
include_directories(include)

# Static Library Target (Freestanding Core)
add_library(superunicode_static STATIC
    src/sutf_encode.c
    src/sutf_decode.c
    src/sucs_string.c
    src/sucs_trap.c
    src/sucs_ucd_names.c
    src/sucs_conv.c
)

set_target_properties(superunicode_static PROPERTIES
    OUTPUT_NAME superunicode_static
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)

# Apply freestanding flags to core library
if(MSVC)
    target_compile_options(superunicode_static PRIVATE /X)
else()
    target_compile_options(superunicode_static PRIVATE -std=c99 -nostdlib -ffreestanding)
endif()

# Subdirectories
enable_testing()
add_subdirectory(tests)
add_subdirectory(tools)

