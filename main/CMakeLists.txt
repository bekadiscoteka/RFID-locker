idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES rc522 wifi nvs_flash)
