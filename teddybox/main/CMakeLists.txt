set(
    COMPONENT_SRCS "main.c" "playback.c" "wifi.c" "webserver.c" "accel.c" 
    EMBED_FILES "favicon.ico" "upload_script.html")
    
set(COMPONENT_ADD_INCLUDEDIRS .)

register_component()
