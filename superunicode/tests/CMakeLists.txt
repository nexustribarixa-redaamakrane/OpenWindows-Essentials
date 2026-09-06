add_executable(test_sutf test_sutf.c)
target_link_libraries(test_sutf PRIVATE superunicode_static)
add_test(NAME test_sutf COMMAND test_sutf)

add_executable(test_sucs_planes test_sucs_planes.c)
target_link_libraries(test_sucs_planes PRIVATE superunicode_static)
add_test(NAME test_sucs_planes COMMAND test_sucs_planes)

add_executable(test_conv test_conv.c)
target_link_libraries(test_conv PRIVATE superunicode_static)
add_test(NAME test_conv COMMAND test_conv)

