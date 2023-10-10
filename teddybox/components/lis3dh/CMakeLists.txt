

set(COMPONENT_REQUIRES)
set(COMPONENT_PRIV_REQUIRES driver esp_peripherals)

list(APPEND COMPONENT_ADD_INCLUDEDIRS ./include)
set(COMPONENT_SRCS ./src/lis3dh.c)

register_component()
