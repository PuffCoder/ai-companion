idf_component_register(SRCS "wifi_demo.c"
                    INCLUDE_DIRS "."
                    REQUIRES 
                        wifi_manager
                        led_driver
                        http_server
                    )