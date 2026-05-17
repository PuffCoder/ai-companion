idf_component_register(SRCS "wifi_demo.c"
                    INCLUDE_DIRS "."
                    REQUIRES 
                        esp_wifi
                        nvs_flash
                        esp_event
                        esp_netif
                        
                    )