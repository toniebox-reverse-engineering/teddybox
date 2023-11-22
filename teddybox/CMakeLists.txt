cmake_minimum_required(VERSION 3.5)

set(COMPONENT_PRIV_REQUIRES driver esp_timer)
set(EXTRA_COMPONENT_DIRS "components")
include($ENV{ADF_PATH}/CMakeLists.txt)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(teddybox)
