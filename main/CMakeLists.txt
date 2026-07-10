idf_component_register(
    SRCS "main.c" "display.c" "gps.c" "map_client.c" "wifi.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_lcd esp_http_client esp_wifi nvs_flash esp_event esp_timer json
    PRIV_REQUIRES gc9a01
)
