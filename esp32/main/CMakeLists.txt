idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES bsp led_driver emotion_engine servo_driver uart_bridge
             app_state stt llm memory
             espressif__esp-sr espressif__cjson
             esp_driver_i2s esp_driver_gpio
             esp_wifi esp_netif esp_event nvs_flash esp_timer freertos
)
