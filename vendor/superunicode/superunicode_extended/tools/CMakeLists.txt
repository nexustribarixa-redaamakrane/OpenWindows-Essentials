# Extended Encoding Conversion CLI Tools
add_executable(ext_utf2sutf utf2sutf.c)
target_link_libraries(ext_utf2sutf superunicode_extended_static)
set_target_properties(ext_utf2sutf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)

add_executable(ext_sutf2utf sutf2utf.c)
target_link_libraries(ext_sutf2utf superunicode_extended_static)
set_target_properties(ext_sutf2utf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)

add_executable(ext_unicode2superunicode unicode2superunicode.c)
target_link_libraries(ext_unicode2superunicode superunicode_extended_static)
set_target_properties(ext_unicode2superunicode PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)

add_executable(ext_superunicode2unicode superunicode2unicode.c)
target_link_libraries(ext_superunicode2unicode superunicode_extended_static)
set_target_properties(ext_superunicode2unicode PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)
