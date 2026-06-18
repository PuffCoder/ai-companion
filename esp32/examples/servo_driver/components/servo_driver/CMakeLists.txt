idf_component_register(
    SRCS "servo_driver.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_ledc bsp log freertos
)
