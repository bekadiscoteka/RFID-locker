# The app core: who is allowed (cards), what happened (accesslog),
# and what a decision does (access_event).
idf_component_register(SRCS "access.c" "accesslog.c" "cards.c" "uid.c"
                       INCLUDE_DIRS "include"
                       REQUIRES rc522
                       PRIV_REQUIRES esp_driver_gpio esp_timer nvs_flash)
