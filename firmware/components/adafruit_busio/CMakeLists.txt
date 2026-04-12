file(GLOB ADAFRUIT_BUSIO_SRCS
  "Adafruit_BusIO/*.cpp"
)

idf_component_register(
    SRCS ${ADAFRUIT_BUSIO_SRCS}
    INCLUDE_DIRS "Adafruit_BusIO"
    REQUIRES arduino-esp32
)
