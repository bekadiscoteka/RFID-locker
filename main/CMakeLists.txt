idf_component_register(SRCS "rc.c" "rc522_spi.c"
                    INCLUDE_DIRS "." REQUIRES esp_timer freertos driver esp_driver_spi esp_driver_gpio)
