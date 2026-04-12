file(GLOB ADAFRUIT_GFX_SRCS
    "Adafruit-GFX/*.cpp"
)

idf_component_register(
    SRCS ${ADAFRUIT_GFX_SRCS}
    INCLUDE_DIRS "Adafruit-GFX"
    REQUIRES arduino-esp32 adafruit_busio
)
