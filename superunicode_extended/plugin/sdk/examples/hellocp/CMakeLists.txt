cmake_minimum_required(VERSION 3.10)
project(hellocp C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include_directories(
    ${SUCS_ROOT}/superunicode_extended/plugin/include
    ${SUCS_ROOT}/superunicode_extended/include
)

add_library(hellocp STATIC
    src/plugin_entry.c
    src/plugin_data.c
)

# Pack into a blob:
#   plugin_pack org.openwindows.hellocp 1 0 0 ranges.txt payload.bin hellocp.sucsplugin
