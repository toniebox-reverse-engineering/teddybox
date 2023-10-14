
set(
    COMPONENT_SRCS "main.c" "nfc.c" "ota.c" "playback.c" "wifi.c" "webserver.c" "accel.c" "proto/protobuf-c.c" "proto/proto/toniebox.pb.taf-header.pb-c.c"
    EMBED_FILES "favicon.ico" "upload_script.html")
    
set(COMPONENT_ADD_INCLUDEDIRS . "proto" "proto/proto")

register_component()
