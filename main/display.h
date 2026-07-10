#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240
#define DISPLAY_CENTER_X 120
#define DISPLAY_CENTER_Y 120
#define DISPLAY_RADIUS   120

esp_err_t display_init(void);
void display_draw_map(const uint16_t *rgb565_pixels);
void display_draw_player_arrow(float heading_deg);
void display_draw_loading(void);
void display_flush(void);
