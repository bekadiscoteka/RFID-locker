idf_component_register(SRCS "main.c" "access.c" "accesslog.c" "web.c" "cards.c"
                       INCLUDE_DIRS "."
                       EMBED_TXTFILES "index.html"
                       REQUIRES rc522 wifi nvs_flash esp_driver_gpio esp_timer
                                esp_netif esp_http_server)
