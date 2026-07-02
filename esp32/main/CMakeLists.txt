idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES led_driver wifi_manager display_driver uart_console servo_driver freertos
)
