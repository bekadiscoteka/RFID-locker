idf_component_register(SRCS "rc.c"
                    INCLUDE_DIRS "." REQUIRES freertos driver esp_driver_spi esp_driver_gpio)
