cmake_minimum_required(VERSION 3.10)
project(superunicode_ecosystem_compat C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ---------------------------------------------------------------------------
# Ecosystem Compatibility Suite
#
# Cross-project verification against the sibling OpenWindows repos. Each
# test co-includes this workspace's public headers with one sibling project's
# headers in a single translation unit (the guarded-constants coexistence
# contract), then asserts runtime parity of the shared contracts:
#
#   BANcode             - Kernel Security Trap dispatch & registry geometry
#   OpenWindows-Storage - OWFS/USFS layout constants & SUTF-8 codec contract
#   vip                 - codec / trap-mapping / addressing parity
#   Modular-Bootloader  - vendored sutf copies, boot-config ABI, OWFS mirrors
#
# Build mode: the documented "real sibling" mode links ../<name> live sources
# directly and defines COMPAT_*_REAL accordingly. When a sibling is not
# checked out next to this workspace (the committed layout), the suite falls
# back to the vendored ports under compat/<name>/ and stays fully buildable.
# Sibling sources are compiled directly into the tests (source-level
# integration, mirroring how the kernel will consume them).
# ---------------------------------------------------------------------------

set(SUCS_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../superunicode/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../sutf/include
)

# Canonical libsutf codec implementation (re-linked for the tests that call
# sutf8_encode_char / sutf8_decode_char / sutf8_codepoint_length).
set(SUTF8_C ${CMAKE_CURRENT_SOURCE_DIR}/../sutf/src/sutf8.c)

# Real sibling roots (checked out next to this workspace, when present)
set(REAL_BANCODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../BANcode)
set(REAL_STORAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../OpenWindows-Storage)
set(REAL_MBL_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../Modular-Bootloader)
set(REAL_VIP_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/../vip)

# Vendored port roots (always present)
set(VENDOR_BANCODE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/BANcode)
set(VENDOR_STORAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/OpenWindows-storage)
set(VENDOR_MBL_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/Modular-Bootloader)
set(VENDOR_VIP_DIR     ${CMAKE_CURRENT_SOURCE_DIR}/vip)

# Source root selection: real sibling wins when present.
if(EXISTS ${REAL_BANCODE_DIR}/kernel_src/bancode/bancode.c)
    set(BANCODE_SRC_DIR ${REAL_BANCODE_DIR})
    message(STATUS "compat_bancode: using REAL ../BANcode kernel sources")
else()
    set(BANCODE_SRC_DIR ${VENDOR_BANCODE_DIR})
    message(STATUS "compat_bancode: using vendored compat/BANcode port")
endif()

if(EXISTS ${REAL_STORAGE_DIR}/common/src/ow_string.c)
    set(STORAGE_SRC_DIR ${REAL_STORAGE_DIR})
    message(STATUS "compat_storage: using REAL ../OpenWindows-Storage common sources")
else()
    set(STORAGE_SRC_DIR ${VENDOR_STORAGE_DIR})
    message(STATUS "compat_storage: using vendored compat/OpenWindows-storage port")
endif()

if(EXISTS ${REAL_MBL_DIR}/src/sutf/sucs_mode.c)
    set(MBL_SRC_DIR ${REAL_MBL_DIR})
    message(STATUS "compat_mbl: using REAL ../Modular-Bootloader sources")
else()
    set(MBL_SRC_DIR ${VENDOR_MBL_DIR})
    message(STATUS "compat_mbl: using vendored compat/Modular-Bootloader port")
endif()

if(EXISTS ${REAL_VIP_DIR}/univip_fvip.c)
    set(VIP_SRC_DIR ${REAL_VIP_DIR})
    message(STATUS "compat_vip: using REAL ../vip sources")
else()
    set(VIP_SRC_DIR ${VENDOR_VIP_DIR})
    message(STATUS "compat_vip: using vendored compat/vip port")
endif()

enable_testing()

# --- BANcode: registry constants + Kernel Security Trap dispatch parity ----
add_executable(test_compat_bancode
    test_compat_bancode.c
    ${SUTF8_C}
    ${BANCODE_SRC_DIR}/kernel_src/bancode/bancode.c
    ${BANCODE_SRC_DIR}/kernel_src/bancode/bancode_trap.c
)
target_include_directories(test_compat_bancode PRIVATE
    ${SUCS_INCLUDE_DIRS}
    ${BANCODE_SRC_DIR}/kernel_inc
)
if(EXISTS ${REAL_BANCODE_DIR}/kernel_src/bancode/bancode.c)
    target_compile_definitions(test_compat_bancode PRIVATE COMPAT_BANCODE_REAL=1)
endif()
add_test(NAME compat_bancode COMMAND test_compat_bancode)

# --- OpenWindows-Storage: layout constants + SUTF-8 codec contract ---------
add_executable(test_compat_storage
    test_compat_storage.c
    ${SUTF8_C}
    ${STORAGE_SRC_DIR}/common/src/ow_string.c
    ${STORAGE_SRC_DIR}/common/src/ow_mem.c
)
target_include_directories(test_compat_storage PRIVATE
    ${SUCS_INCLUDE_DIRS}
    ${STORAGE_SRC_DIR}/common/include
    ${STORAGE_SRC_DIR}/owfs/include
    ${STORAGE_SRC_DIR}/usfs/include
)
if(EXISTS ${REAL_STORAGE_DIR}/common/src/ow_string.c)
    target_compile_definitions(test_compat_storage PRIVATE COMPAT_STORAGE_REAL=1)
endif()
add_test(NAME compat_storage COMMAND test_compat_storage)

# --- vip: codec / trap-mapping / volume-addressing parity ------------------
add_executable(test_compat_vip
    test_compat_vip.c
    compat_tu_fsfirst.c
    ${SUTF8_C}
    ${VIP_SRC_DIR}/univip_fvip.c
)
target_include_directories(test_compat_vip PRIVATE
    ${SUCS_INCLUDE_DIRS}
    ${BANCODE_SRC_DIR}/kernel_inc
    ${VIP_SRC_DIR}
    ${STORAGE_SRC_DIR}/owfs/include
    ${STORAGE_SRC_DIR}/usfs/include
)
target_compile_definitions(test_compat_vip PRIVATE
    COMPAT_HAVE_BANCODE=1
    COMPAT_HAVE_OWFS=1
)
add_test(NAME compat_vip COMMAND test_compat_vip)

# --- Modular-Bootloader: vendored sutf copies + boot-config ABI ------------
add_executable(test_compat_mbl
    test_compat_mbl.c
    ${SUTF8_C}
    ${MBL_SRC_DIR}/src/sutf/sucs_mode.c
)
target_include_directories(test_compat_mbl PRIVATE
    ${MBL_SRC_DIR}/include
    ${BANCODE_SRC_DIR}/kernel_inc
    ${SUCS_INCLUDE_DIRS}
)
add_test(NAME compat_mbl COMMAND test_compat_mbl)

set_target_properties(
    test_compat_bancode test_compat_storage test_compat_vip test_compat_mbl
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin
)