#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "display.h"
#include "gps.h"
#include "map_client.h"
#include "wifi.h"
#include <math.h>
#include <string.h>

static const char *TAG = "vice_city";

#define MIN_MOVE_METERS 5.0

static double s_last_lat = 0;
static double s_last_lon = 0;

static double haversine_m(double lat1, double lon1, double lat2, double lon2)
{
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat / 2) * sin(dlat / 2) +
               cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
               sin(dlon / 2) * sin(dlon / 2);
    return 6371000.0 * 2.0 * atan2(sqrt(a), sqrt(1 - a));
}

static void map_task(void *arg)
{
    uint16_t *tile_buf = heap_caps_malloc(MAP_TILE_WIDTH * MAP_TILE_HEIGHT * 2, MALLOC_CAP_SPIRAM);
    if (!tile_buf) {
        ESP_LOGE(TAG, "Failed to allocate tile buffer");
        vTaskDelete(NULL);
        return;
    }

    bool first_fix = true;

    while (1) {
        gps_data_t gps = gps_get_data();

        if (!gps.valid) {
            ESP_LOGW(TAG, "Waiting for GPS fix...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        double dist = haversine_m(s_last_lat, s_last_lon, gps.latitude, gps.longitude);
        bool should_fetch = first_fix || dist > MIN_MOVE_METERS;

        if (should_fetch) {
            ESP_LOGI(TAG, "Fetching map at %.6f, %.6f (sats: %d)",
                     gps.latitude, gps.longitude, gps.satellites);

            esp_err_t err = map_client_fetch_tile(
                gps.latitude, gps.longitude,
                CONFIG_MAP_ZOOM_LEVEL, tile_buf);

            if (err == ESP_OK) {
                display_draw_map(tile_buf);
                s_last_lat = gps.latitude;
                s_last_lon = gps.longitude;
                first_fix = false;
            } else {
                ESP_LOGE(TAG, "Tile fetch failed");
            }
        }

        display_draw_player_arrow(gps.heading);
        display_flush();

        vTaskDelay(pdMS_TO_TICKS(CONFIG_MAP_FETCH_INTERVAL_MS));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Vice City Map Navigator ===");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    ESP_ERROR_CHECK(display_init());
    ESP_LOGI(TAG, "Display ready");

    ESP_ERROR_CHECK(gps_init());
    ESP_LOGI(TAG, "GPS ready");

    ESP_ERROR_CHECK(wifi_init());
    ESP_LOGI(TAG, "WiFi connected");

    ESP_ERROR_CHECK(map_client_init());
    ESP_LOGI(TAG, "Map client ready");

    xTaskCreatePinnedToCore(map_task, "map", 8192, NULL, 4, NULL, 0);
}
