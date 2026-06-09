idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES espressif__esp-sr esp_driver_i2s
)
