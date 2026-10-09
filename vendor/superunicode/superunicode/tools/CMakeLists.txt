# Inspector CLI tool
add_executable(sucs_inspector sucs_inspector.c)
target_link_libraries(sucs_inspector PRIVATE superunicode_static)
set_target_properties(sucs_inspector PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)

# Encoding conversion CLI tools
add_executable(utf2sutf utf2sutf.c)
target_link_libraries(utf2sutf PRIVATE superunicode_static)
set_target_properties(utf2sutf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)

add_executable(sutf2utf sutf2utf.c)
target_link_libraries(sutf2utf PRIVATE superunicode_static)
set_target_properties(sutf2utf PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)

add_executable(unicode2superunicode unicode2superunicode.c)
target_link_libraries(unicode2superunicode PRIVATE superunicode_static)
set_target_properties(unicode2superunicode PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)

add_executable(superunicode2unicode superunicode2unicode.c)
target_link_libraries(superunicode2unicode PRIVATE superunicode_static)
set_target_properties(superunicode2unicode PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../bin
)
