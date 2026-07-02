idf_component_register(
    SRCS "console.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart wifi_manager lwip log
)
