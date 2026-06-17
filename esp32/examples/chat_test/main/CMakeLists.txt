idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES espressif__esp-sr esp_driver_i2s esp_wifi esp_netif
             esp_event nvs_flash esp_timer stt llm memory
)
