
option(DEVBOARD "Compile for devboard" OFF)



set(COMPONENT_SRCS "main.c" "helpers.c" "ledman.c" "cloud.c" "malloc.c" "nfc.c" "ota.c" "playback.c" "wifi.c" "webserver.c" "accel.c" "proto/protobuf-c.c" "proto/proto/toniebox.pb.taf-header.pb-c.c")
set(COMPONENT_ADD_INCLUDEDIRS . "proto" "proto/proto")

set(EMBED_FILES "favicon.ico" "upload_script.html")

if(DEVBOARD)
    add_definitions(-DDEVBOARD)
    idf_build_set_property(COMPILE_DEFINITIONS "-DDEVBOARD" APPEND)
    list(APPEND EMBED_FILES "00000000")
endif()

set(COMPONENT_EMBED_FILES ${EMBED_FILES})

register_component()
