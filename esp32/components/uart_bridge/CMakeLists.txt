idf_component_register(
    SRCS "uart_bridge.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart esp_driver_gpio bsp log freertos
)
