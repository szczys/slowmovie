cmake_minimum_required(VERSION 4.2)

list(APPEND EXTRA_COMPONENT_DIRS components/golioth-firmware-sdk/port/esp_idf/components)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(slowmovie)
