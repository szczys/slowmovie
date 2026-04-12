set(srcs
    "epaper.cpp"
    "main.cpp"
    "golioth.c"
    "wifi.c"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 gxepd2 golioth_sdk adafruit_busio adafruit_gfx
)
