# HTTP server: the page in index.html and the JSON API it uses.
idf_component_register(SRCS "web.c"
                       INCLUDE_DIRS "include"
                       EMBED_TXTFILES "index.html"
                       PRIV_REQUIRES access esp_http_server)
