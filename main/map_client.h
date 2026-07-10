#pragma once

#include "esp_err.h"
#include <stdint.h>

#define MAP_TILE_WIDTH  240
#define MAP_TILE_HEIGHT 240

esp_err_t map_client_init(void);
esp_err_t map_client_fetch_tile(double lat, double lon, int zoom, uint16_t *rgb565_out);
