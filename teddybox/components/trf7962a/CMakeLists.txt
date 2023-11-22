

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES driver esp_timer)

list(APPEND COMPONENT_ADD_INCLUDEDIRS ./include)
set(COMPONENT_SRCS ./src/trf7962a.c)

register_component()
