#include "display.h"
#include "gc9a01.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>

static const char *TAG = "display";

static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_panel_io;
static uint16_t *s_framebuf;

#define RGB565(r, g, b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))

static const uint16_t COLOR_BG         = RGB565(0x0A, 0x06, 0x1A);
static const uint16_t COLOR_PLAYER     = RGB565(0xFF, 0xFF, 0xFF);
static const uint16_t COLOR_PLAYER_OUT = RGB565(0xFF, 0x6E, 0xC7);
static const uint16_t COLOR_LOADING_BG = RGB565(0x1A, 0x0A, 0x2E);
static const uint16_t COLOR_LOADING_FG = RGB565(0x00, 0xFF, 0xE5);

static inline bool pixel_in_circle(int x, int y)
{
    int dx = x - DISPLAY_CENTER_X;
    int dy = y - DISPLAY_CENTER_Y;
    return (dx * dx + dy * dy) <= (DISPLAY_RADIUS * DISPLAY_RADIUS);
}

esp_err_t display_init(void)
{
    esp_err_t ret = gc9a01_init(&s_panel, &s_panel_io);
    if (ret != ESP_OK) return ret;

    s_framebuf = heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * 2, MALLOC_CAP_SPIRAM);
    if (!s_framebuf) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer in PSRAM");
        s_framebuf = heap_caps_malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * 2, MALLOC_CAP_DMA);
    }
    if (!s_framebuf) return ESP_ERR_NO_MEM;

    memset(s_framebuf, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
    display_draw_loading();
    display_flush();

    ESP_LOGI(TAG, "Display initialized");
    return ESP_OK;
}

void display_draw_map(const uint16_t *rgb565_pixels)
{
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            int idx = y * DISPLAY_WIDTH + x;
            if (pixel_in_circle(x, y)) {
                s_framebuf[idx] = rgb565_pixels[idx];
            } else {
                s_framebuf[idx] = COLOR_BG;
            }
        }
    }
}

static void draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color)
{
    int min_y = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
    int max_y = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    if (min_y < 0) min_y = 0;
    if (max_y >= DISPLAY_HEIGHT) max_y = DISPLAY_HEIGHT - 1;

    for (int y = min_y; y <= max_y; y++) {
        int nodes[3];
        int n = 0;
        int pts_x[] = {x0, x1, x2, x0};
        int pts_y[] = {y0, y1, y2, y0};
        for (int i = 0; i < 3; i++) {
            int ay = pts_y[i], by = pts_y[i + 1];
            int ax = pts_x[i], bx = pts_x[i + 1];
            if ((ay <= y && by > y) || (by <= y && ay > y)) {
                nodes[n++] = ax + (y - ay) * (bx - ax) / (by - ay);
            }
        }
        if (n >= 2) {
            if (nodes[0] > nodes[1]) { int t = nodes[0]; nodes[0] = nodes[1]; nodes[1] = t; }
            for (int x = nodes[0]; x <= nodes[1]; x++) {
                if (x >= 0 && x < DISPLAY_WIDTH) {
                    s_framebuf[y * DISPLAY_WIDTH + x] = color;
                }
            }
        }
    }
}

void display_draw_player_arrow(float heading_deg)
{
    float rad = heading_deg * M_PI / 180.0f;
    int cx = DISPLAY_CENTER_X;
    int cy = DISPLAY_CENTER_Y;
    int arrow_len = 10;
    int arrow_half_w = 5;

    float tip_x = cx + arrow_len * sinf(rad);
    float tip_y = cy - arrow_len * cosf(rad);
    float left_x = cx - arrow_half_w * cosf(rad) + (arrow_len * 0.4f) * sinf(rad + M_PI);
    float left_y = cy - arrow_half_w * sinf(rad) - (arrow_len * 0.4f) * cosf(rad + M_PI);
    float right_x = cx + arrow_half_w * cosf(rad) + (arrow_len * 0.4f) * sinf(rad + M_PI);
    float right_y = cy + arrow_half_w * sinf(rad) - (arrow_len * 0.4f) * cosf(rad + M_PI);

    draw_filled_triangle(
        (int)tip_x, (int)tip_y,
        (int)left_x, (int)left_y,
        (int)right_x, (int)right_y,
        COLOR_PLAYER_OUT
    );

    int inset = 2;
    float itip_x = cx + (arrow_len - inset) * sinf(rad);
    float itip_y = cy - (arrow_len - inset) * cosf(rad);
    float ileft_x = cx - (arrow_half_w - inset) * cosf(rad) + ((arrow_len - inset) * 0.4f) * sinf(rad + M_PI);
    float ileft_y = cy - (arrow_half_w - inset) * sinf(rad) - ((arrow_len - inset) * 0.4f) * cosf(rad + M_PI);
    float iright_x = cx + (arrow_half_w - inset) * cosf(rad) + ((arrow_len - inset) * 0.4f) * sinf(rad + M_PI);
    float iright_y = cy + (arrow_half_w - inset) * sinf(rad) - ((arrow_len - inset) * 0.4f) * cosf(rad + M_PI);

    draw_filled_triangle(
        (int)itip_x, (int)itip_y,
        (int)ileft_x, (int)ileft_y,
        (int)iright_x, (int)iright_y,
        COLOR_PLAYER
    );
}

void display_draw_loading(void)
{
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
        for (int x = 0; x < DISPLAY_WIDTH; x++) {
            int idx = y * DISPLAY_WIDTH + x;
            if (pixel_in_circle(x, y)) {
                int dx = x - DISPLAY_CENTER_X;
                int dy = y - DISPLAY_CENTER_Y;
                int dist = (int)sqrtf(dx * dx + dy * dy);
                if (dist > DISPLAY_RADIUS - 3 && dist <= DISPLAY_RADIUS) {
                    s_framebuf[idx] = COLOR_LOADING_FG;
                } else {
                    s_framebuf[idx] = COLOR_LOADING_BG;
                }
            } else {
                s_framebuf[idx] = COLOR_BG;
            }
        }
    }

    int cross_size = 8;
    for (int i = -cross_size; i <= cross_size; i++) {
        int cx = DISPLAY_CENTER_X + i;
        int cy = DISPLAY_CENTER_Y;
        if (cx >= 0 && cx < DISPLAY_WIDTH)
            s_framebuf[cy * DISPLAY_WIDTH + cx] = COLOR_LOADING_FG;
        cx = DISPLAY_CENTER_X;
        cy = DISPLAY_CENTER_Y + i;
        if (cy >= 0 && cy < DISPLAY_HEIGHT)
            s_framebuf[cy * DISPLAY_WIDTH + cx] = COLOR_LOADING_FG;
    }
}

void display_flush(void)
{
    esp_lcd_panel_draw_bitmap(s_panel, 0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, s_framebuf);
}
