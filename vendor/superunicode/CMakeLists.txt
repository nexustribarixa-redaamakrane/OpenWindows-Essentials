cmake_minimum_required(VERSION 3.10)
project(SuperUnicodeWorkspace C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Enable testing at root before adding subdirectories
enable_testing()

# Export compile commands so IDEs/language servers (clangd) can resolve the
# project include paths for accurate diagnostics.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Specification & Architecture Include Paths ────────────────────
# These paths expose the SUAS/SUTS headers for IDE resolution and
# future conformance tests. They do NOT add build targets — the
# headers are spec-only and carry no compiled artefacts.

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ── Documentation Verification ────────────────────────────────────
# Lightweight custom target that verifies the doc scaffold integrity.
# Runs via `cmake --build . --target verify-docs` but does NOT compile
# any source files. Uses its own CMake script for cross-platform
# file-presence checks.

add_custom_target(verify-docs
    COMMENT "Verifying documentation scaffold..."
    COMMAND ${CMAKE_COMMAND}
        -DDOCS_ROOT_DIR=${CMAKE_CURRENT_SOURCE_DIR}/docs
        -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_docs.cmake
)

# ── SUAS Architecture Standards (SDF, SGW, SBR, SUCF) ───────────
# Freestanding static library implementing Structural Directional Framing
# and the full Unicode Bidirectional Algorithm (UAX #9) processing model,
# System Glyph Width & Monospace Grid (SGW) fixed-cell terminal metrics,
# System Boundary & Line Break Rules (SBR) single-pass line breaking,
# and SuperUnicode Canonical Forms (SUCF) single-pass canonical transforms.
add_library(suas_static STATIC
    src/suas/suas_sdf.c
    src/suas/suas_sgw.c
    src/suas/suas_sbr.c
    src/suas/suas_sucf.c
)

target_include_directories(suas_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(suas_static PRIVATE /X)
else()
    target_compile_options(suas_static PRIVATE -std=c99 -nostdlib -ffreestanding)
endif()

# SUAS host unit tests
add_executable(test_suas_sdf tests/test_suas_sdf.c)
target_link_libraries(test_suas_sdf PRIVATE suas_static)
add_test(NAME test_suas_sdf COMMAND test_suas_sdf)

add_executable(test_suas_sgw tests/test_suas_sgw.c)
target_link_libraries(test_suas_sgw PRIVATE suas_static)
add_test(NAME test_suas_sgw COMMAND test_suas_sgw)

add_executable(test_suas_sbr tests/test_suas_sbr.c)
target_link_libraries(test_suas_sbr PRIVATE suas_static)
add_test(NAME test_suas_sbr COMMAND test_suas_sbr)

add_executable(test_suas_sucf tests/test_suas_sucf.c)
target_link_libraries(test_suas_sucf PRIVATE suas_static)
add_test(NAME test_suas_sucf COMMAND test_suas_sucf)

# ── SUTS-001 SuperUnicode Collation Algorithm (SUCA) ────────────────
# Freestanding static library; UCA(UTS #10)-equivalent multilevel
# collation over the 64-bit SUCS char space (extSUCS compatible).
add_library(suts_static STATIC
    src/suts/suts_suca.c
)

target_include_directories(suts_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(MSVC)
    target_compile_options(suts_static PRIVATE /X)
else()
    target_compile_options(suts_static PRIVATE -std=c99 -nostdlib -ffreestanding)
endif()

# SUTS host unit tests
add_executable(test_suts_suca tests/test_suts_suca.c)
target_link_libraries(test_suts_suca PRIVATE suts_static)
add_test(NAME test_suts_suca COMMAND test_suts_suca)

# ── Sub-projects ──────────────────────────────────────────────────
add_subdirectory(superunicode)
add_subdirectory(sutf)
add_subdirectory(superunicode_extended)
add_subdirectory(sust)
add_subdirectory(suf)

# Unified header-coexistence test (all module headers in one TU)
add_subdirectory(unified)

