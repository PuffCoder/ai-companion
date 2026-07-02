idf_component_register(
    SRCS "llm.c"
    INCLUDE_DIRS "."
    REQUIRES esp_http_client esp-tls espressif__cjson log
)
