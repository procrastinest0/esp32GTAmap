#pragma once

#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    double latitude;
    double longitude;
    float speed_knots;
    float heading;
    bool valid;
    int satellites;
} gps_data_t;

esp_err_t gps_init(void);
gps_data_t gps_get_data(void);
