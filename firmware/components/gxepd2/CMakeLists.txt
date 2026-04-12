file(GLOB_RECURSE GXEPD2_SRCS
    "*.cpp"
    "*.c"
)

idf_component_register(
    SRCS ${GXEPD2_SRCS}
    INCLUDE_DIRS "GxEPD2/src"
    REQUIRES arduino-esp32 adafruit_gfx
)
