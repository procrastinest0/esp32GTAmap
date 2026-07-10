#include "map_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_jpeg_dec.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "map_client";

#define MAX_JPEG_SIZE (100 * 1024)

static uint8_t *s_jpeg_buf;
static int s_jpeg_len;

/* Vice City color scheme for Google Maps Static API */
static const char *VICE_CITY_STYLES =
    "&style=feature:water|color:0x0d4f4f"
    "&style=feature:landscape|color:0x1a0a2e"
    "&style=feature:landscape.man_made|color:0x1f0e35"
    "&style=feature:road|element:geometry.fill|color:0xff6ec7"
    "&style=feature:road|element:geometry.stroke|color:0xc7548f"
    "&style=feature:road.highway|element:geometry.fill|color:0xff3da5"
    "&style=feature:road|element:labels|visibility:off"
    "&style=feature:poi|visibility:off"
    "&style=feature:transit|visibility:off"
    "&style=feature:administrative|element:geometry|color:0x2d1050"
    "&style=feature:administrative|element:labels|visibility:off"
    "&style=element:labels|visibility:off";

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (s_jpeg_len + evt->data_len <= MAX_JPEG_SIZE) {
            memcpy(s_jpeg_buf + s_jpeg_len, evt->data, evt->data_len);
            s_jpeg_len += evt->data_len;
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

esp_err_t map_client_init(void)
{
    s_jpeg_buf = heap_caps_malloc(MAX_JPEG_SIZE, MALLOC_CAP_SPIRAM);
    if (!s_jpeg_buf) {
        s_jpeg_buf = malloc(MAX_JPEG_SIZE);
    }
    if (!s_jpeg_buf) return ESP_ERR_NO_MEM;
    ESP_LOGI(TAG, "Map client initialized");
    return ESP_OK;
}

esp_err_t map_client_fetch_tile(double lat, double lon, int zoom, uint16_t *rgb565_out)
{
    char url[1024];
    snprintf(url, sizeof(url),
        "https://maps.googleapis.com/maps/api/staticmap"
        "?center=%.6f,%.6f"
        "&zoom=%d"
        "&size=%dx%d"
        "&scale=1"
        "&format=jpg"
        "&maptype=roadmap"
        "%s"
        "&key=%s",
        lat, lon, zoom,
        MAP_TILE_WIDTH, MAP_TILE_HEIGHT,
        VICE_CITY_STYLES,
        CONFIG_GOOGLE_MAPS_API_KEY);

    s_jpeg_len = 0;

    esp_http_client_config_t http_cfg = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .disable_auto_redirect = false,
    };

    esp_http_client_handle_t client = esp_http_client_init(&http_cfg);
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status != 200) {
        ESP_LOGE(TAG, "HTTP status %d", status);
        return ESP_FAIL;
    }

    if (s_jpeg_len == 0) {
        ESP_LOGE(TAG, "Empty response");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Received %d bytes JPEG", s_jpeg_len);

    jpeg_dec_config_t dec_cfg = {
        .output_type = JPEG_RAW_TYPE_RGB565_BE,
    };
    jpeg_dec_handle_t dec = NULL;
    err = jpeg_dec_open(&dec_cfg, &dec);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decoder open failed");
        return err;
    }

    jpeg_dec_io_t dec_io = {
        .inbuf = s_jpeg_buf,
        .inbuf_len = s_jpeg_len,
    };
    jpeg_dec_header_info_t header;
    err = jpeg_dec_parse_header(dec, &dec_io, &header);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "JPEG header parse failed");
        jpeg_dec_close(dec);
        return err;
    }

    dec_io.outbuf = (uint8_t *)rgb565_out;
    err = jpeg_dec_process(dec, &dec_io);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "JPEG decode failed");
    }

    jpeg_dec_close(dec);
    return err;
}
