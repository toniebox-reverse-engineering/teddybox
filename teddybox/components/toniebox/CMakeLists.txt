# Edit following two lines to set component requirements (see docs)
set(COMPONENT_REQUIRES lis3dh trf7962a)
set(COMPONENT_PRIV_REQUIRES audio_sal audio_hal esp_dispatcher esp_peripherals esp_adc display_service)

if(CONFIG_AUDIO_BOARD_CUSTOM)
message(STATUS "Current board name is " CONFIG_AUDIO_BOARD_CUSTOM)
list(APPEND COMPONENT_ADD_INCLUDEDIRS ./toniebox_esp32_v1.6.C ./dac3100)
set(COMPONENT_SRCS
./toniebox_esp32_v1.6.C/led.c
./toniebox_esp32_v1.6.C/adc.c
./toniebox_esp32_v1.6.C/board.c
./toniebox_esp32_v1.6.C/board_pins_config.c
./dac3100/dac3100.c
)
endif()

register_component()

IF (IDF_VERSION_MAJOR GREATER 3)
idf_component_get_property(audio_board_lib audio_board COMPONENT_LIB)
set_property(TARGET ${audio_board_lib} APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${COMPONENT_LIB})

ELSEIF (IDF_VERSION_MAJOR EQUAL 3)
set_property(TARGET idf_component_audio_board APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES  $<TARGET_PROPERTY:${COMPONENT_TARGET},INTERFACE_INCLUDE_DIRECTORIES>)

ENDIF (IDF_VERSION_MAJOR GREATER 3)
