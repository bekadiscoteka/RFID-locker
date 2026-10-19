# Boot sequence and the RFID loop: the only place that knows about every component.
idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES rc522 wifi access web nvs_flash esp_netif esp_timer)
