set(srcs
    "epaper.cpp"
    "credentials.c"
    "console.c"
    "fw_update.c"
    "main.cpp"
    "golioth.c"
    "wifi.c"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES arduino-esp32 gxepd2 golioth_sdk adafruit_busio adafruit_gfx app_update
)
