#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

ifdef CONFIG_AUDIO_BOARD_CUSTOM
COMPONENT_ADD_INCLUDEDIRS += ./dac3100
COMPONENT_SRCDIRS += ./dac3100

COMPONENT_ADD_INCLUDEDIRS += ./toniebox_esp32_v1.6.C
COMPONENT_SRCDIRS += ./toniebox_esp32_v1.6.C
endif