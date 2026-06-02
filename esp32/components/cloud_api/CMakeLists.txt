idf_component_register(
    SRCS "cloud_api.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client espressif__cjson nvs_flash log
)
