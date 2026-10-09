# Inspector CLI
add_executable(suf_inspector suf_inspector.c)
target_link_libraries(suf_inspector suf_static)

# Inbound converters
add_executable(ttf2suf ttf2suf.c)
target_link_libraries(ttf2suf suf_static)

add_executable(otf2suf otf2suf.c)
target_link_libraries(otf2suf suf_static)

add_executable(woff2suf woff2suf.c)
target_link_libraries(woff2suf suf_static)

add_executable(sfd2suf sfd2suf.c)
target_link_libraries(sfd2suf suf_static)

add_executable(eot2suf eot2suf.c)
target_link_libraries(eot2suf suf_static)

add_executable(ps2suf ps2suf.c)
target_link_libraries(ps2suf suf_static)

add_executable(pfb2suf pfb2suf.c)
target_link_libraries(pfb2suf suf_static)

# Outbound exporters
add_executable(suf2ttf suf2ttf.c)
target_link_libraries(suf2ttf suf_static)

add_executable(suf2otf suf2otf.c)
target_link_libraries(suf2otf suf_static)

add_executable(suf2woff suf2woff.c)
target_link_libraries(suf2woff suf_static)

add_executable(suf2sfd suf2sfd.c)
target_link_libraries(suf2sfd suf_static)

add_executable(suf2eot suf2eot.c)
target_link_libraries(suf2eot suf_static)

add_executable(suf2ps suf2ps.c)
target_link_libraries(suf2ps suf_static)

add_executable(suf2pfb suf2pfb.c)
target_link_libraries(suf2pfb suf_static)

add_executable(ufo2suf ufo2suf.c)
target_link_libraries(ufo2suf suf_static)

add_executable(suf2ufo suf2ufo.c)
target_link_libraries(suf2ufo suf_static)

# Plugin fontmaking CLI tool
add_executable(suf_plugin_tool suf_plugin_tool.c)
target_link_libraries(suf_plugin_tool suf_static)
