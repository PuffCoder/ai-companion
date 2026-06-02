idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES app_state led_driver wifi_manager cloud_api display_driver freertos
)
