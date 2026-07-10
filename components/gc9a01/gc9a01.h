#pragma once

#include "esp_lcd_types.h"
#include "esp_err.h"

#define GC9A01_WIDTH  240
#define GC9A01_HEIGHT 240

esp_err_t gc9a01_init(esp_lcd_panel_handle_t *panel, esp_lcd_panel_io_handle_t *io);
