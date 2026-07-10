#include "gps.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "gps";

static gps_data_t s_gps_data;
static portMUX_TYPE s_gps_lock = portMUX_INITIALIZER_UNLOCKED;

static double nmea_to_decimal(const char *raw, const char *dir)
{
    if (!raw || !dir || raw[0] == '\0') return 0.0;
    double val = atof(raw);
    int degrees = (int)(val / 100);
    double minutes = val - degrees * 100;
    double decimal = degrees + minutes / 60.0;
    if (*dir == 'S' || *dir == 'W') decimal = -decimal;
    return decimal;
}

static char *next_field(char *p)
{
    while (*p && *p != ',') p++;
    if (*p == ',') { *p = '\0'; return p + 1; }
    return p;
}

static void parse_gga(char *sentence)
{
    char *p = sentence;
    char *fields[15];
    int n = 0;
    fields[n++] = p;
    while (*p && n < 15) {
        if (*p == ',') {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    if (n < 10) return;

    int quality = atoi(fields[6]);
    if (quality == 0) return;

    double lat = nmea_to_decimal(fields[2], fields[3]);
    double lon = nmea_to_decimal(fields[4], fields[5]);
    int sats = atoi(fields[7]);

    portENTER_CRITICAL(&s_gps_lock);
    s_gps_data.latitude = lat;
    s_gps_data.longitude = lon;
    s_gps_data.satellites = sats;
    s_gps_data.valid = true;
    portEXIT_CRITICAL(&s_gps_lock);
}

static void parse_rmc(char *sentence)
{
    char *p = sentence;
    char *fields[15];
    int n = 0;
    fields[n++] = p;
    while (*p && n < 15) {
        if (*p == ',') {
            *p = '\0';
            fields[n++] = p + 1;
        }
        p++;
    }
    if (n < 9) return;
    if (fields[2][0] != 'A') return;

    double lat = nmea_to_decimal(fields[3], fields[4]);
    double lon = nmea_to_decimal(fields[5], fields[6]);
    float speed = atof(fields[7]);
    float heading = atof(fields[8]);

    portENTER_CRITICAL(&s_gps_lock);
    s_gps_data.latitude = lat;
    s_gps_data.longitude = lon;
    s_gps_data.speed_knots = speed;
    s_gps_data.heading = heading;
    s_gps_data.valid = true;
    portEXIT_CRITICAL(&s_gps_lock);
}

static void parse_nmea_line(char *line)
{
    if (line[0] != '$') return;

    char *star = strchr(line, '*');
    if (star) *star = '\0';

    if (strstr(line, "GGA")) {
        char *payload = strchr(line, ',');
        if (payload) parse_gga(payload + 1);
    } else if (strstr(line, "RMC")) {
        char *payload = strchr(line, ',');
        if (payload) parse_rmc(payload + 1);
    }
}

static void gps_task(void *arg)
{
    uint8_t buf[512];
    char line[256];
    int line_pos = 0;

    while (1) {
        int len = uart_read_bytes(CONFIG_GPS_UART_PORT, buf, sizeof(buf) - 1, pdMS_TO_TICKS(100));
        if (len <= 0) continue;

        for (int i = 0; i < len; i++) {
            char c = (char)buf[i];
            if (c == '\n' || c == '\r') {
                if (line_pos > 0) {
                    line[line_pos] = '\0';
                    parse_nmea_line(line);
                    line_pos = 0;
                }
            } else if (line_pos < (int)sizeof(line) - 1) {
                line[line_pos++] = c;
            }
        }
    }
}

esp_err_t gps_init(void)
{
    uart_config_t uart_cfg = {
        .baud_rate = CONFIG_GPS_UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    ESP_ERROR_CHECK(uart_param_config(CONFIG_GPS_UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(CONFIG_GPS_UART_PORT,
                                 CONFIG_GPS_UART_TX, CONFIG_GPS_UART_RX,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(CONFIG_GPS_UART_PORT, 1024, 0, 0, NULL, 0));

    xTaskCreatePinnedToCore(gps_task, "gps", 4096, NULL, 5, NULL, 1);
    ESP_LOGI(TAG, "GPS initialized on UART%d (TX:%d RX:%d @ %d baud)",
             CONFIG_GPS_UART_PORT, CONFIG_GPS_UART_TX, CONFIG_GPS_UART_RX, CONFIG_GPS_UART_BAUD);
    return ESP_OK;
}

gps_data_t gps_get_data(void)
{
    gps_data_t data;
    portENTER_CRITICAL(&s_gps_lock);
    data = s_gps_data;
    portEXIT_CRITICAL(&s_gps_lock);
    return data;
}
