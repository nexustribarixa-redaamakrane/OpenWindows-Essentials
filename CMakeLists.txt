cmake_minimum_required(VERSION 3.20)
project(OpenWindows-Essentials LANGUAGES C)

# ====================================================================
#  Global Compiler Configuration (C99 Freestanding)
# ====================================================================

add_compile_options(
    -std=c99
    -ffreestanding
    -nostdlib
    -Wall
    -Wextra
    -Werror
    -Wpedantic
)

add_compile_definitions(
    __freestanding__
    __OW_ESSENTIALS__
)

# ====================================================================
#  Root Include Paths
# ====================================================================

include_directories(
    ${CMAKE_SOURCE_DIR}/Extensions
    ${CMAKE_SOURCE_DIR}/Extensions/damage_control
    ${CMAKE_SOURCE_DIR}/DLL/owrp
    ${CMAKE_SOURCE_DIR}/DLL/kernel64
    ${CMAKE_SOURCE_DIR}/DLL/htl
    ${CMAKE_SOURCE_DIR}/DLL/bootvid
    ${CMAKE_SOURCE_DIR}/DLL/suct
    ${CMAKE_SOURCE_DIR}/DLL/sched64
    ${CMAKE_SOURCE_DIR}/DLL/vmm64
    ${CMAKE_SOURCE_DIR}/DLL/unitrans64
    ${CMAKE_SOURCE_DIR}/DLL/kconf64
    ${CMAKE_SOURCE_DIR}/DLL/owwm64
    ${CMAKE_SOURCE_DIR}/DLL/sufrender
    ${CMAKE_SOURCE_DIR}/DLL/packmgr
    ${CMAKE_SOURCE_DIR}/DLL/sentinel64
    ${CMAKE_SOURCE_DIR}/DLL/taskmgr64
    ${CMAKE_SOURCE_DIR}/Drivers/fltrmgr
    ${CMAKE_SOURCE_DIR}/Drivers/classpnp
    ${CMAKE_SOURCE_DIR}/Drivers/banhammer
    ${CMAKE_SOURCE_DIR}/Drivers/sio
    ${CMAKE_SOURCE_DIR}/Drivers/vipm
    ${CMAKE_SOURCE_DIR}/Drivers/ps2hid
    ${CMAKE_SOURCE_DIR}/Drivers/pitapic
    ${CMAKE_SOURCE_DIR}/Drivers/vgapci
    ${CMAKE_SOURCE_DIR}/Drivers/owfsdrv
    ${CMAKE_SOURCE_DIR}/Drivers/nullzero
    ${CMAKE_SOURCE_DIR}/Drivers/rtc
    ${CMAKE_SOURCE_DIR}/Software
)

# ====================================================================
#  Custom Output Naming
# ====================================================================
#
# OpenWindows binaries keep their exact names - no MinGW "lib" prefix and
# no doubled extension:
#   - libraries (DLL/)     -> X.owd   (target name X.owd, PREFIX "" and
#                                      OUTPUT_NAME X keep it single)
#   - components (Drivers/)-> X.owc   (executables; the PE entry comes
#                                      from the shared owc_entry stub,
#                                      so no CRT startup is dragged in)
#   - executables (Software/)-> X.owx (native executables with owx_entry)

set(OWC_ENTRY_C ${CMAKE_SOURCE_DIR}/Drivers/owc_entry.c)
set(OWD_ENTRY_C ${CMAKE_SOURCE_DIR}/DLL/owd_entry.c)
set(OWX_ENTRY_C ${CMAKE_SOURCE_DIR}/Software/owx_entry.c)

# libgcc as a real (late) link entry so its members (___chkstk_ms, mul
# helpers) are pulled after the module objects - -lgcc would sit before
# them and never resolve those references.
execute_process(COMMAND ${CMAKE_C_COMPILER} -print-libgcc-file-name
    OUTPUT_VARIABLE OW_LIBGCC OUTPUT_STRIP_TRAILING_WHITESPACE)

function(ow_freestanding target)
    target_sources(${target} PRIVATE ${OWD_ENTRY_C})
    # No CRT startup (no DllMainCRTStartup -> no msvcrt/KERNEL32 imports).
    target_link_options(${target} PRIVATE
        -nostdlib "-Wl,-e,owd_entry_stub" "-Wl,--export-all-symbols")
    if(OW_LIBGCC)
        target_link_libraries(${target} PRIVATE "${OW_LIBGCC}")
    endif()
endfunction()

function(ow_owd_props target outdir)
    string(REGEX REPLACE "\\.owd$" "" _base "${target}")
    set_target_properties(${target} PROPERTIES
        PREFIX ""
        OUTPUT_NAME "${_base}"
        SUFFIX ".owd"
        LINKER_LANGUAGE C
        LIBRARY_OUTPUT_DIRECTORY "${outdir}"
        RUNTIME_OUTPUT_DIRECTORY "${outdir}"
    )
endfunction()

function(ow_owc_props target outdir)
    string(REGEX REPLACE "\\.owc$" "" _base "${target}")
    target_sources(${target} PRIVATE ${OWC_ENTRY_C})
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${_base}"
        SUFFIX ".owc"
        LINKER_LANGUAGE C
        RUNTIME_OUTPUT_DIRECTORY "${outdir}"
    )
    # Components are still growing: unused statics (helpers, entry points
    # of not-yet-wired drivers) are warnings, not errors.
    target_compile_options(${target} PRIVATE -Wno-error=unused-function)
    # No CRT startup (no WinMain); libgcc is pulled late via library link.
    target_link_options(${target} PRIVATE
        -nostdlib "-Wl,-e,owc_entry_stub")
    if(OW_LIBGCC)
        target_link_libraries(${target} PRIVATE "${OW_LIBGCC}")
    endif()
endfunction()

function(ow_owx_props target outdir)
    string(REGEX REPLACE "\\.owx$" "" _base "${target}")
    target_sources(${target} PRIVATE ${OWX_ENTRY_C})
    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME "${_base}"
        SUFFIX ".owx"
        LINKER_LANGUAGE C
        RUNTIME_OUTPUT_DIRECTORY "${outdir}"
    )
    target_compile_options(${target} PRIVATE -Wno-error=unused-function)
    target_link_options(${target} PRIVATE
        -nostdlib "-Wl,-e,owx_entry_stub")
    if(OW_LIBGCC)
        target_link_libraries(${target} PRIVATE "${OW_LIBGCC}")
    endif()
endfunction()

# ====================================================================
#  Drivers/ - Bare-metal hardware drivers (.owc)
#  Each subdirectory produces a .owc binary.
#  Sources are added when explicitly specified.
# ====================================================================

# --- Filter Manager (fltrmgr.owc) ---
# Request interceptor for storage stack events.
set(FLTRMGR_SOURCES
    ${CMAKE_SOURCE_DIR}/Drivers/fltrmgr/fltrmgr.c
    CACHE STRING "Source files for fltrmgr.owc"
)
add_executable(fltrmgr.owc ${FLTRMGR_SOURCES})
ow_owc_props(fltrmgr.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- PnP Arbiter (classpnp.owc) ---
# Plug-and-Play Arbiter and class driver bus negotiator.
set(CLASSPNP_SOURCES
    ${CMAKE_SOURCE_DIR}/Drivers/classpnp/classpnp.c
    CACHE STRING "Source files for classpnp.owc"
)
add_executable(classpnp.owc ${CLASSPNP_SOURCES})
ow_owc_props(classpnp.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- Banhammer (banhammer.owc) ---
# Kernel panic handler and emergency execution engine.
set(BANHAMMER_SOURCES
    ${CMAKE_SOURCE_DIR}/Drivers/banhammer/banhammer.c
    ${CMAKE_SOURCE_DIR}/Drivers/banhammer/banhammer_config.c
    CACHE STRING "Source files for banhammer.owc"
)
add_executable(banhammer.owc ${BANHAMMER_SOURCES})
ow_owc_props(banhammer.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- Serial I/O (sio.owc) ---
# 16550 UART driver: ring-buffered polled/IRQ character I/O, SUTF-8 path.
set(SIO_SOURCES
    ${CMAKE_SOURCE_DIR}/Drivers/sio/sio.c
    CACHE STRING "Source files for sio.owc"
)
add_executable(sio.owc ${SIO_SOURCES})
ow_owc_props(sio.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- Volume Index Partition Manager (vipm.owc) ---
# Sparse 44-bit hex trie (UniVIP) + FVIP hot-path volume index driver.
set(VIPM_SOURCES
    ${CMAKE_SOURCE_DIR}/Drivers/vipm/vipm.c
    CACHE STRING "Source files for vipm.owc"
)
add_executable(vipm.owc ${VIPM_SOURCES})
ow_owc_props(vipm.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- PS/2 Input Controller (ps2hid.owc) ---
set(PS2HID_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/ps2hid/ps2hid.c CACHE STRING "Source files for ps2hid.owc")
add_executable(ps2hid.owc ${PS2HID_SOURCES})
ow_owc_props(ps2hid.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- Timer Subsystem (pitapic.owc) ---
set(PITAPIC_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/pitapic/pitapic.c CACHE STRING "Source files for pitapic.owc")
add_executable(pitapic.owc ${PITAPIC_SOURCES})
ow_owc_props(pitapic.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- PCI Framebuffer (vgapci.owc) ---
set(VGAPCI_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/vgapci/vgapci.c CACHE STRING "Source files for vgapci.owc")
add_executable(vgapci.owc ${VGAPCI_SOURCES})
ow_owc_props(vgapci.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- OWFS Native Storage Driver (owfsdrv.owc) ---
set(OWFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/owfsdrv/owfsdrv.c CACHE STRING "Source files for owfsdrv.owc")
add_executable(owfsdrv.owc ${OWFSDRV_SOURCES})
ow_owc_props(owfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- Pseudo Devices (nullzero.owc) ---
set(NULLZERO_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/nullzero/nullzero.c CACHE STRING "Source files for nullzero.owc")
add_executable(nullzero.owc ${NULLZERO_SOURCES})
ow_owc_props(nullzero.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- Real-Time Clock (rtc.owc) ---
set(RTC_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/rtc/rtc.c CACHE STRING "Source files for rtc.owc")
add_executable(rtc.owc ${RTC_SOURCES})
ow_owc_props(rtc.owc "${CMAKE_BINARY_DIR}/Drivers")

# --- Universal Filesystem Drivers (.owc) ---
set(NTFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/ntfsdrv/ntfsdrv.c CACHE STRING "Source files for ntfsdrv.owc")
add_executable(ntfsdrv.owc ${NTFSDRV_SOURCES})
ow_owc_props(ntfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(FATDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/fatdrv/fatdrv.c CACHE STRING "Source files for fatdrv.owc")
add_executable(fatdrv.owc ${FATDRV_SOURCES})
ow_owc_props(fatdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(EXFATDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/exfatdrv/exfatdrv.c CACHE STRING "Source files for exfatdrv.owc")
add_executable(exfatdrv.owc ${EXFATDRV_SOURCES})
ow_owc_props(exfatdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(EXT4DRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/ext4drv/ext4drv.c CACHE STRING "Source files for ext4drv.owc")
add_executable(ext4drv.owc ${EXT4DRV_SOURCES})
ow_owc_props(ext4drv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(BTRFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/btrfsdrv/btrfsdrv.c CACHE STRING "Source files for btrfsdrv.owc")
add_executable(btrfsdrv.owc ${BTRFSDRV_SOURCES})
ow_owc_props(btrfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(XFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/xfsdrv/xfsdrv.c CACHE STRING "Source files for xfsdrv.owc")
add_executable(xfsdrv.owc ${XFSDRV_SOURCES})
ow_owc_props(xfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(ZFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/zfsdrv/zfsdrv.c CACHE STRING "Source files for zfsdrv.owc")
add_executable(zfsdrv.owc ${ZFSDRV_SOURCES})
ow_owc_props(zfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(APFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/apfsdrv/apfsdrv.c CACHE STRING "Source files for apfsdrv.owc")
add_executable(apfsdrv.owc ${APFSDRV_SOURCES})
ow_owc_props(apfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(HFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/hfsdrv/hfsdrv.c CACHE STRING "Source files for hfsdrv.owc")
add_executable(hfsdrv.owc ${HFSDRV_SOURCES})
ow_owc_props(hfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(CDFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/cdfsdrv/cdfsdrv.c CACHE STRING "Source files for cdfsdrv.owc")
add_executable(cdfsdrv.owc ${CDFSDRV_SOURCES})
ow_owc_props(cdfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(UFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/ufsdrv/ufsdrv.c CACHE STRING "Source files for ufsdrv.owc")
add_executable(ufsdrv.owc ${UFSDRV_SOURCES})
ow_owc_props(ufsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(REFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/refsdrv/refsdrv.c CACHE STRING "Source files for refsdrv.owc")
add_executable(refsdrv.owc ${REFSDRV_SOURCES})
ow_owc_props(refsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(F2FSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/f2fsdrv/f2fsdrv.c CACHE STRING "Source files for f2fsdrv.owc")
add_executable(f2fsdrv.owc ${F2FSDRV_SOURCES})
ow_owc_props(f2fsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(UDFDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/udfdrv/udfdrv.c CACHE STRING "Source files for udfdrv.owc")
add_executable(udfdrv.owc ${UDFDRV_SOURCES})
ow_owc_props(udfdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(SQUASHFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/squashfsdrv/squashfsdrv.c CACHE STRING "Source files for squashfsdrv.owc")
add_executable(squashfsdrv.owc ${SQUASHFSDRV_SOURCES})
ow_owc_props(squashfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

set(JFSDRV_SOURCES ${CMAKE_SOURCE_DIR}/Drivers/jfsdrv/jfsdrv.c CACHE STRING "Source files for jfsdrv.owc")
add_executable(jfsdrv.owc ${JFSDRV_SOURCES})
ow_owc_props(jfsdrv.owc "${CMAKE_BINARY_DIR}/Drivers")

# ====================================================================
#  DLL/ - Dynamic link libraries (.owd)
#  Each subdirectory produces a .owd library.
#  Sources are added when explicitly specified.
# ====================================================================

# --- Hardware Translation Layer (htl.owd) ---
set(HTL_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/htl/htl.c
    CACHE STRING "Source files for htl.owd"
)
add_library(htl.owd SHARED ${HTL_SOURCES})
ow_freestanding(htl.owd)
ow_owd_props(htl.owd "${CMAKE_BINARY_DIR}/DLL")

# --- OpenWindows Ring Portal (owrp.owd) ---
set(OWRP_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/owrp/owrp.c
    CACHE STRING "Source files for owrp.owd"
)
add_library(owrp.owd SHARED ${OWRP_SOURCES})
ow_freestanding(owrp.owd)
ow_owd_props(owrp.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Boot Video (bootvid.owd) ---
set(BOOTVID_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/bootvid/bootvid.c
    CACHE STRING "Source files for bootvid.owd"
)
add_library(bootvid.owd SHARED ${BOOTVID_SOURCES})
ow_freestanding(bootvid.owd)
ow_owd_props(bootvid.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Kernel64 Base API (kernel64.owd) ---
set(KERNEL64_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/kernel64/kernel64.c
    CACHE STRING "Source files for kernel64.owd"
)
add_library(kernel64.owd SHARED ${KERNEL64_SOURCES})
target_link_libraries(kernel64.owd PRIVATE owrp.owd)
ow_freestanding(kernel64.owd)
ow_owd_props(kernel64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- SuperUnicode Text Services (suct.owd) ---
# PUA glyph zones, legacy->SUCS codecs, OWM text plane, OWRP console sink.
set(SUCT_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/suct/suct.c
    CACHE STRING "Source files for suct.owd"
)
add_library(suct.owd SHARED ${SUCT_SOURCES})
target_link_libraries(suct.owd PRIVATE owrp.owd)
ow_freestanding(suct.owd)
ow_owd_props(suct.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Core Runtime Environment (owcrt64.owd) ---
# Freestanding C/C++-payload runtime: segregated heap over k64 memory,
# static TLS slots, atexit table, cxa guards. The foundation the other
# modules build on if they need allocation.
set(CRT64_SOURCES
    ${CMAKE_SOURCE_DIR}/Runtimes/crt64/owcrt64.c
    CACHE STRING "Source files for owcrt64.owd"
)
add_library(owcrt64.owd SHARED ${CRT64_SOURCES})
target_link_libraries(owcrt64.owd PRIVATE kernel64.owd)
ow_freestanding(owcrt64.owd)
ow_owd_props(owcrt64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Encryption Engine (owcrypt.owd) ---
# SHA-256/512, BLAKE2b, HMAC, ChaCha20, Poly1305, AES+GCM, X25519,
# Ed25519, CSPRNG, SUTF security frames. Uses __int128 in the field
# arithmetic, so pedantic mode is relaxed for this target only.
set(OWCRYPT_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/owcrypt/owcrypt.c
    CACHE STRING "Source files for owcrypt.owd"
)
add_library(owcrypt.owd SHARED ${OWCRYPT_SOURCES})
target_compile_options(owcrypt.owd PRIVATE -Wno-pedantic)
target_link_libraries(owcrypt.owd PRIVATE kernel64.owd)
ow_freestanding(owcrypt.owd)
ow_owd_props(owcrypt.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Session / Crypto-Link Layer (netwerk.owd) ---
# Endpoint/link registry, keep-alive heartbeat state machine, SUTF-sealed
# link frames with a replay window and ingress BAN pre-scan. Depends on
# owcrypt.owd for the sealed framing.
set(NETWERK_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/netwerk/netwerk.c
    CACHE STRING "Source files for netwerk.owd"
)
add_library(netwerk.owd SHARED ${NETWERK_SOURCES})
target_link_libraries(netwerk.owd PRIVATE owcrypt.owd kernel64.owd)
ow_freestanding(netwerk.owd)
ow_owd_props(netwerk.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Managed Language Engines bundle (engines.owd) ---
# Language/component registry + 8 census slots, 5 engines delivered
# (poco, managednet, manageddl, managedcom, managedsh) and 3 reserved
# interpreters (managedpy / managedjs / managedlua -> DEPENDENCY until
# implemented). Fully self-contained: no kernel64, no CRT, no heap.
set(ENGINES_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/engines/engines.c
    CACHE STRING "Source files for engines.owd"
)
add_library(engines.owd SHARED ${ENGINES_SOURCES})
ow_freestanding(engines.owd)
ow_owd_props(engines.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Genuine Cairo core (cairo64.owd) ---
# Bundles the unmodified genuine cairo-1.18.5 core (libcairo64_vendor.a)
# over pixman-0.46.5 (libpixman_vendor.a) on the owrt freestanding runtime
# shim (libowrt64.a). All three are produced by their build drivers, then
# linked here into a single .owd substituting cairo_*. The k64 memory API
# is imported from kernel64.owd at load time.
set(CAIRO64_OWRT_PS1  ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/crotrt.ps1)
set(CAIRO64_PIX_PS1   ${CMAKE_SOURCE_DIR}/vendor/pixman/ow-build/cpixman.ps1)
set(CAIRO64_CAI_PS1   ${CMAKE_SOURCE_DIR}/vendor/cairo/ow-build/ccairo.ps1)
set(CAIRO64_OWRT_A    ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/build/libowrt64.a)
set(CAIRO64_PIX_A     ${CMAKE_SOURCE_DIR}/vendor/pixman/ow-build/build/libpixman_vendor.a)
set(CAIRO64_CAI_A     ${CMAKE_SOURCE_DIR}/vendor/cairo/ow-build/build/libcairo64_vendor.a)

add_custom_command(OUTPUT ${CAIRO64_OWRT_A}
    COMMAND pwsh -NoProfile -File ${CAIRO64_OWRT_PS1}
    DEPENDS ${CAIRO64_OWRT_PS1}
            ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/ow_runtime.h
            ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/ow_alloc.c
            ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/ow_str.c
            ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/ow_fmt.c
            ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/ow_math.c
            ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/ow_crt.c
            ${CMAKE_SOURCE_DIR}/DLL/cairo64/owrt/ow_jmp.s
    COMMENT "Building owrt freestanding runtime shim"
)

add_custom_command(OUTPUT ${CAIRO64_PIX_A}
    COMMAND pwsh -NoProfile -File ${CAIRO64_PIX_PS1}
    DEPENDS ${CAIRO64_PIX_PS1}
    COMMENT "Building genuine pixman vendor archive"
)

add_custom_command(OUTPUT ${CAIRO64_CAI_A}
    COMMAND pwsh -NoProfile -File ${CAIRO64_CAI_PS1}
    DEPENDS ${CAIRO64_CAI_PS1}
    COMMENT "Building genuine cairo vendor archive"
)

add_custom_target(cairo64_vendor DEPENDS
    ${CAIRO64_OWRT_A} ${CAIRO64_PIX_A} ${CAIRO64_CAI_A}
)

set(CAIRO64_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/cairo64/cairo64.c
    CACHE STRING "Source files for cairo64.owd"
)
add_library(cairo64.owd SHARED ${CAIRO64_SOURCES})
target_include_directories(cairo64.owd PRIVATE ${CMAKE_SOURCE_DIR}/DLL/cairo64)
target_link_libraries(cairo64.owd PRIVATE
    "-Wl,--whole-archive"
    ${CAIRO64_CAI_A} ${CAIRO64_PIX_A} ${CAIRO64_OWRT_A}
    "-Wl,--no-whole-archive"
    kernel64.owd
)
target_link_options(cairo64.owd PRIVATE
    -nostdlib
    "-Wl,-e,owd64_dll_ep"
    "-Wl,--export-all-symbols"
)
add_dependencies(cairo64.owd cairo64_vendor)
ow_owd_props(cairo64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- SUTF text layer on Cairo (cairoplus64.owd) ---
# Implements the SUCS cell-glyph text layer over the cairo64 ABI:
# the module's own code (cairoplus64.c) is linked with -nostdlib against
# the cairo64 import library and kernel64. It uses no libc of its own
# (all cairo allocations go through cairo64's owrt allocator).
set(CAIROPLUS64_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/cairoplus64/cairoplus64.c
)
add_library(cairoplus64.owd SHARED ${CAIROPLUS64_SOURCES})
target_include_directories(cairoplus64.owd PRIVATE
    ${CMAKE_SOURCE_DIR}/DLL/cairoplus64
    ${CMAKE_SOURCE_DIR}/Extensions
    ${CMAKE_SOURCE_DIR}/DLL/kernel64
)
target_link_libraries(cairoplus64.owd PRIVATE
    cairo64.owd
    kernel64.owd
)
target_link_options(cairoplus64.owd PRIVATE
    -nostdlib
    "-Wl,-e,owdplus64_dll_ep"
    "-Wl,--export-all-symbols"
)
ow_owd_props(cairoplus64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Compositor / controls (userinterface64.owd) ---
# ARGB32 window back-buffers over cairo64 image surfaces + clipped
# source-over composite (window_present). Freestanding like the other
# Cairo modules: imports cairo64.owd and kernel64.owd only.
set(USERINTERFACE64_SOURCES
    ${CMAKE_SOURCE_DIR}/DLL/userinterface64/userinterface64.c
)
add_library(userinterface64.owd SHARED ${USERINTERFACE64_SOURCES})
target_include_directories(userinterface64.owd PRIVATE
    ${CMAKE_SOURCE_DIR}/DLL/userinterface64
    ${CMAKE_SOURCE_DIR}/Extensions
    ${CMAKE_SOURCE_DIR}/DLL/kernel64
)
target_link_libraries(userinterface64.owd PRIVATE
    cairo64.owd
    kernel64.owd
)
target_link_options(userinterface64.owd PRIVATE
    -nostdlib
    "-Wl,-e,owui64_dll_ep"
    "-Wl,--export-all-symbols"
)
ow_owd_props(userinterface64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Thread Scheduler (sched64.owd) ---
set(SCHED64_SOURCES ${CMAKE_SOURCE_DIR}/DLL/sched64/sched64.c CACHE STRING "Source files for sched64.owd")
add_library(sched64.owd SHARED ${SCHED64_SOURCES})
ow_freestanding(sched64.owd)
ow_owd_props(sched64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Virtual Memory Manager (vmm64.owd) ---
set(VMM64_SOURCES ${CMAKE_SOURCE_DIR}/DLL/vmm64/vmm64.c CACHE STRING "Source files for vmm64.owd")
add_library(vmm64.owd SHARED ${VMM64_SOURCES})
ow_freestanding(vmm64.owd)
ow_owd_props(vmm64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Unicode Translation Layer (unitrans64.owd) ---
set(UNITRANS64_SOURCES ${CMAKE_SOURCE_DIR}/DLL/unitrans64/unitrans64.c CACHE STRING "Source files for unitrans64.owd")
add_library(unitrans64.owd SHARED ${UNITRANS64_SOURCES})
ow_freestanding(unitrans64.owd)
ow_owd_props(unitrans64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Configuration Engine (kconf64.owd) ---
set(KCONF64_SOURCES ${CMAKE_SOURCE_DIR}/DLL/kconf64/kconf64.c CACHE STRING "Source files for kconf64.owd")
add_library(kconf64.owd SHARED ${KCONF64_SOURCES})
ow_freestanding(kconf64.owd)
ow_owd_props(kconf64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Window Management (owwm64.owd) ---
set(OWWM64_SOURCES ${CMAKE_SOURCE_DIR}/DLL/owwm64/owwm64.c CACHE STRING "Source files for owwm64.owd")
add_library(owwm64.owd SHARED ${OWWM64_SOURCES})
ow_freestanding(owwm64.owd)
ow_owd_props(owwm64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- SuperUnicode Font Rasterizer (sufrender.owd) ---
set(SUFRENDER_SOURCES ${CMAKE_SOURCE_DIR}/DLL/sufrender/sufrender.c CACHE STRING "Source files for sufrender.owd")
add_library(sufrender.owd SHARED ${SUFRENDER_SOURCES})
ow_freestanding(sufrender.owd)
ow_owd_props(sufrender.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Package & Resource Manager (packmgr.owd) ---
set(PACKMGR_SOURCES ${CMAKE_SOURCE_DIR}/DLL/packmgr/packmgr.c CACHE STRING "Source files for packmgr.owd")
add_library(packmgr.owd SHARED ${PACKMGR_SOURCES})
ow_freestanding(packmgr.owd)
ow_owd_props(packmgr.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Sentinel Recovery Watchdog (sentinel64.owd) ---
set(SENTINEL64_SOURCES ${CMAKE_SOURCE_DIR}/DLL/sentinel64/sentinel64.c CACHE STRING "Source files for sentinel64.owd")
add_library(sentinel64.owd SHARED ${SENTINEL64_SOURCES})
ow_freestanding(sentinel64.owd)
ow_owd_props(sentinel64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Process Lifecycle Manager (taskmgr64.owd) ---
set(TASKMGR64_SOURCES ${CMAKE_SOURCE_DIR}/DLL/taskmgr64/taskmgr64.c CACHE STRING "Source files for taskmgr64.owd")
add_library(taskmgr64.owd SHARED ${TASKMGR64_SOURCES})
ow_freestanding(taskmgr64.owd)
ow_owd_props(taskmgr64.owd "${CMAKE_BINARY_DIR}/DLL")

# --- Universal Filesystem Switch (owfs64.owd) ---
set(OWFS64_SOURCES ${CMAKE_SOURCE_DIR}/DLL/owfs64/owfs64.c CACHE STRING "Source files for owfs64.owd")
add_library(owfs64.owd SHARED ${OWFS64_SOURCES})
ow_freestanding(owfs64.owd)
ow_owd_props(owfs64.owd "${CMAKE_BINARY_DIR}/DLL")

# ====================================================================
#  Extensions/ - Sentinel engine and format parsers
# ====================================================================

set(DAMAGE_CONTROL_SOURCES
    ${CMAKE_SOURCE_DIR}/Extensions/damage_control/damage_control.c
)

add_library(damage_control STATIC ${DAMAGE_CONTROL_SOURCES})
target_include_directories(damage_control PUBLIC
    ${CMAKE_SOURCE_DIR}/Extensions
    ${CMAKE_SOURCE_DIR}/Extensions/damage_control
)
set_target_properties(damage_control PROPERTIES
    LINKER_LANGUAGE C
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Extensions"
)

# ====================================================================
#  Software/ - Native Executables (.owx)
# ====================================================================

add_executable(owinit.owx ${CMAKE_SOURCE_DIR}/Software/owinit/owinit.c)
target_link_libraries(owinit.owx PRIVATE kconf64.owd)
ow_owx_props(owinit.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(owsh.owx ${CMAKE_SOURCE_DIR}/Software/owsh/owsh.c)
ow_owx_props(owsh.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(owedit.owx ${CMAKE_SOURCE_DIR}/Software/owedit/owedit.c)
ow_owx_props(owedit.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(owkill.owx ${CMAKE_SOURCE_DIR}/Software/owkill/owkill.c)
target_link_libraries(owkill.owx PRIVATE taskmgr64.owd sentinel64.owd)
ow_owx_props(owkill.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(owwm.owx ${CMAKE_SOURCE_DIR}/Software/owwm/owwm.c)
target_link_libraries(owwm.owx PRIVATE owwm64.owd)
ow_owx_props(owwm.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(startwm.owx ${CMAKE_SOURCE_DIR}/Software/startwm/startwm.c)
target_link_libraries(startwm.owx PRIVATE owwm64.owd)
ow_owx_props(startwm.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(win.owx ${CMAKE_SOURCE_DIR}/Software/win/win.c ${CMAKE_SOURCE_DIR}/Software/startwm/startwm.c)
target_link_libraries(win.owx PRIVATE owwm64.owd)
ow_owx_props(win.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(banhammer_cli.owx ${CMAKE_SOURCE_DIR}/Software/banhammer/banhammer_cli.c)
ow_owx_props(banhammer_cli.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(bancat.owx ${CMAKE_SOURCE_DIR}/Software/bancat/bancat.c)
ow_owx_props(bancat.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(vipmount.owx ${CMAKE_SOURCE_DIR}/Software/vipmount/vipmount.c)
ow_owx_props(vipmount.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(sfontview.owx ${CMAKE_SOURCE_DIR}/Software/sfontview/sfontview.c)
target_link_libraries(sfontview.owx PRIVATE sufrender.owd)
ow_owx_props(sfontview.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(kconfctl.owx ${CMAKE_SOURCE_DIR}/Software/kconfctl/kconfctl.c)
target_link_libraries(kconfctl.owx PRIVATE kconf64.owd)
ow_owx_props(kconfctl.owx "${CMAKE_BINARY_DIR}/Software")

add_executable(kextctl.owx ${CMAKE_SOURCE_DIR}/Software/kextctl/kextctl.c)
ow_owx_props(kextctl.owx "${CMAKE_BINARY_DIR}/Software")

# ====================================================================
#  Install Rules (placeholder)
# ====================================================================

install(TARGETS damage_control
    ARCHIVE DESTINATION lib
)

# Format headers are header-only; no install rule needed for them.
install(FILES
    Extensions/owc_format.h
    Extensions/owd_format.h
    Extensions/sentinel_format.h
    DESTINATION include
)

# ====================================================================
#  Vendor: Canonical SuperUnicode workspace (self-contained project)
# ====================================================================
# The canonical SuperUnicode workspace lives under vendor/superunicode and is
# a separate CMake project. It is NOT added as a subdirectory: its hosted
# tests and tools link the CRT, which conflicts with the global freestanding
# flags applied above. Instead it is exposed as an isolated ExternalProject
# target. Enable the option, then:
#
#     cmake --build <build-dir> --target superunicode_vendor
#
# which configures, compiles, and runs the canonical ctest suite in
# <build-dir>/vendor. This build never contaminates the module include path
# and is never linked into any .owc/.owd target.
include(ExternalProject)
option(OW_BUILD_VENDOR_SUPERUNICODE
    "Configure, build, and test the canonical SuperUnicode vendor workspace"
    OFF)
if(OW_BUILD_VENDOR_SUPERUNICODE)
    enable_testing()
    ExternalProject_Add(superunicode_vendor
        SOURCE_DIR  ${CMAKE_SOURCE_DIR}/vendor/superunicode
        BINARY_DIR  ${CMAKE_BINARY_DIR}/vendor
        INSTALL_COMMAND  ""
        CMAKE_ARGS
            "-G${CMAKE_GENERATOR}"
            -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
            -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF
        TEST_COMMAND
            ${CMAKE_CTEST_COMMAND} --test-dir ${CMAKE_BINARY_DIR}/vendor
            --output-on-failure
    )
endif()
