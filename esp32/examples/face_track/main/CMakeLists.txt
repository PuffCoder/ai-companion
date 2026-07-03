idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES uart_bridge servo_driver espressif__cjson
)
