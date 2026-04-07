/**
 * LCD Display Driver for ST7789 240x240 TFT
 * Simple tech-style UI for robot status and dialogue display
 */

#include "lcd_display.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"

static const char *TAG = "lcd";

#define LCD_HOST       SPI3_HOST
#define LCD_PIXEL_CLK  (40 * 1000 * 1000)

#define PIN_SCLK       9
#define PIN_MOSI       10
#define PIN_DC         46
#define PIN_RST        11
#define PIN_CS         12
#define PIN_BL         3

#define LCD_H_RES      240
#define LCD_V_RES      240

#define BG_COLOR       0x0000
#define PRIMARY_COLOR   0x00FF
#define SECONDARY_COLOR 0x0F0F
#define ACCENT_COLOR    0x07FF
#define DIM_COLOR       0x0888

static esp_lcd_panel_handle_t panel_handle = NULL;
static int current_state = STATE_SLEEPING;
static char current_message[256] = {0};

void lcd_backlight_set(bool on)
{
    gpio_set_level(PIN_BL, on ? 1 : 0);
}

static void draw_pixel(int x, int y, uint16_t color)
{
    if (x < 0 || x >= LCD_H_RES || y < 0 || y >= LCD_V_RES) return;
    esp_lcd_panel_draw_bitmap(panel_handle, x, y, x + 1, y + 1, &color);
}

static void draw_rect(int x1, int y1, int x2, int y2, uint16_t color, bool filled)
{
    if (filled) {
        int height = y2 - y1;
        int width = x2 - x1;
        if (width <= 0 || height <= 0) return;
        uint16_t *buffer = heap_caps_malloc(width * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (buffer) {
            for (int i = 0; i < width; i++) buffer[i] = color;
            for (int y = y1; y < y2; y++) {
                esp_lcd_panel_draw_bitmap(panel_handle, x1, y, x2, y + 1, buffer);
            }
            free(buffer);
        }
    } else {
        for (int x = x1; x < x2; x++) {
            draw_pixel(x, y1, color);
            draw_pixel(x, y2 - 1, color);
        }
        for (int y = y1; y < y2; y++) {
            draw_pixel(x1, y, color);
            draw_pixel(x2 - 1, y, color);
        }
    }
}

static void draw_line(int x1, int y1, int x2, int y2, uint16_t color)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        draw_pixel(x1, y1, color);
        if (x1 == x2 && y1 == y2) break;
        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

static const uint8_t FONT_5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, // ' '
    {0x00,0x00,0x2F,0x00,0x00}, // '!'
    {0x00,0x07,0x00,0x07,0x00}, // '"'
    {0x14,0x7F,0x14,0x7F,0x14}, // '#'
    {0x24,0x2E,0x2A,0x2E,0x24}, // '$'
    {0x46,0x26,0x10,0x0D,0x46}, // '%'
    {0x30,0x4D,0x4D,0x40,0xC0}, // '&'
    {0x00,0x00,0x07,0x00,0x00}, // '''
    {0x00,0x1C,0x22,0x41,0x00}, // '('
    {0x00,0x41,0x22,0x1C,0x00}, // ')'
    {0x14,0x08,0x3E,0x08,0x14}, // '*'
    {0x08,0x08,0x3E,0x08,0x08}, // '+'
    {0x00,0x80,0x60,0x00,0x00}, // ','
    {0x08,0x08,0x08,0x08,0x08}, // '-'
    {0x00,0x00,0x60,0x00,0x00}, // '.'
    {0x40,0x20,0x10,0x08,0x04}, // '/'
    {0x3E,0x51,0x49,0x45,0x3E}, // '0'
    {0x00,0x42,0x7F,0x40,0x00}, // '1'
    {0x42,0x61,0x51,0x49,0x46}, // '2'
    {0x21,0x41,0x45,0x4B,0x31}, // '3'
    {0x0C,0x0A,0x08,0x7F,0x08}, // '4'
    {0x27,0x45,0x45,0x45,0x39}, // '5'
    {0x3E,0x49,0x49,0x49,0x26}, // '6'
    {0x01,0x71,0x09,0x05,0x03}, // '7'
    {0x36,0x49,0x49,0x49,0x36}, // '8'
    {0x26,0x49,0x49,0x49,0x3E}, // '9'
    {0x00,0x36,0x36,0x00,0x00}, // ':'
    {0x00,0x80,0x66,0x00,0x00}, // ';'
    {0x08,0x14,0x22,0x41,0x00}, // '<'
    {0x14,0x14,0x14,0x14,0x14}, // '='
    {0x00,0x41,0x22,0x14,0x08}, // '>'
    {0x02,0x01,0x51,0x09,0x06}, // '?'
    {0x3E,0x41,0x5D,0x55,0x5E}, // '@'
    {0x7E,0x09,0x09,0x09,0x7E}, // 'A'
    {0x7F,0x49,0x49,0x49,0x36}, // 'B'
    {0x3E,0x41,0x41,0x41,0x22}, // 'C'
    {0x7F,0x41,0x41,0x22,0x1C}, // 'D'
    {0x7F,0x49,0x49,0x49,0x41}, // 'E'
    {0x7F,0x09,0x09,0x09,0x01}, // 'F'
    {0x3E,0x41,0x49,0x49,0x7A}, // 'G'
    {0x7F,0x08,0x08,0x08,0x7F}, // 'H'
    {0x00,0x41,0x7F,0x41,0x00}, // 'I'
    {0x20,0x40,0x41,0x3F,0x01}, // 'J'
    {0x7F,0x08,0x14,0x22,0x41}, // 'K'
    {0x7F,0x40,0x40,0x40,0x40}, // 'L'
    {0x7F,0x02,0x0C,0x02,0x7F}, // 'M'
    {0x7F,0x04,0x08,0x10,0x7F}, // 'N'
    {0x3E,0x41,0x41,0x41,0x3E}, // 'O'
    {0x7F,0x09,0x09,0x09,0x06}, // 'P'
    {0x3E,0x41,0x51,0x21,0x5E}, // 'Q'
    {0x7F,0x09,0x19,0x29,0x46}, // 'R'
    {0x26,0x49,0x49,0x49,0x32}, // 'S'
    {0x01,0x01,0x7F,0x01,0x01}, // 'T'
    {0x3F,0x40,0x40,0x40,0x3F}, // 'U'
    {0x1F,0x20,0x40,0x20,0x1F}, // 'V'
    {0x3F,0x40,0x38,0x40,0x3F}, // 'W'
    {0x63,0x14,0x08,0x14,0x63}, // 'X'
    {0x07,0x08,0x70,0x08,0x07}, // 'Y'
    {0x61,0x51,0x49,0x45,0x43}, // 'Z'
    {0x00,0x7F,0x41,0x41,0x00}, // '['
    {0x04,0x08,0x10,0x20,0x40}, // '\'
    {0x00,0x41,0x41,0x7F,0x00}, // ']'
    {0x04,0x02,0x01,0x02,0x04}, // '^'
    {0x40,0x40,0x40,0x40,0x40}, // '_'
    {0x00,0x01,0x02,0x04,0x00}, // '`'
    {0x20,0x54,0x54,0x54,0x78}, // 'a'
    {0x7F,0x48,0x44,0x44,0x38}, // 'b'
    {0x38,0x44,0x44,0x44,0x28}, // 'c'
    {0x38,0x44,0x44,0x48,0x7F}, // 'd'
    {0x38,0x54,0x54,0x54,0x18}, // 'e'
    {0x08,0x7E,0x09,0x09,0x02}, // 'f'
    {0x18,0xA4,0xA4,0xA4,0x7C}, // 'g'
    {0x7F,0x08,0x04,0x04,0x78}, // 'h'
    {0x00,0x48,0x7D,0x40,0x00}, // 'i'
    {0x40,0x80,0x48,0x3D,0x00}, // 'j'
    {0x7F,0x10,0x28,0x44,0x00}, // 'k'
    {0x00,0x3E,0x40,0x40,0x00}, // 'l'
    {0x7C,0x04,0x18,0x04,0x78}, // 'm'
    {0x7C,0x08,0x04,0x04,0x78}, // 'n'
    {0x38,0x44,0x44,0x44,0x38}, // 'o'
    {0xFC,0x48,0x44,0x44,0x38}, // 'p'
    {0x38,0x44,0x44,0x48,0xFC}, // 'q'
    {0x44,0x08,0x04,0x04,0x18}, // 'r'
    {0x48,0x54,0x54,0x54,0x24}, // 's'
    {0x04,0x3F,0x44,0x40,0x20}, // 't'
    {0x3C,0x40,0x40,0x20,0x7C}, // 'u'
    {0x1C,0x20,0x40,0x20,0x1C}, // 'v'
    {0x3C,0x40,0x30,0x40,0x3C}, // 'w'
    {0x44,0x28,0x10,0x28,0x44}, // 'x'
    {0x1C,0xA0,0xA0,0xA0,0x7C}, // 'y'
    {0x44,0x64,0x54,0x4C,0x44}, // 'z'
    {0x00,0x08,0x36,0x41,0x00}, // '{'
    {0x00,0x00,0x7F,0x00,0x00}, // '|'
    {0x00,0x41,0x36,0x08,0x00}, // '}'
    {0x10,0x08,0x08,0x10,0x08}, // '~'
    {0x7F,0x7F,0x7F,0x7F,0x7F}, // DEL (filled block)
};

static void draw_char(int x, int y, char c, uint16_t color, uint16_t bg)
{
    if (c < ' ' || c > 'z') c = ' ';
    const uint8_t *glyph = FONT_5x7[c - ' '];

    for (int i = 0; i < 5; i++) {
        uint8_t line = glyph[i];
        for (int j = 0; j < 7; j++) {
            uint16_t col = (line & (1 << j)) ? color : bg;
            draw_pixel(x + i, y + j, col);
        }
    }
}

static void draw_text(int x, int y, const char *text, uint16_t color, uint16_t bg)
{
    if (!text) return;
    int len = strlen(text);
    int cur_x = x;
    for (int i = 0; i < len && cur_x < LCD_H_RES - 5; i++) {
        draw_char(cur_x, y, text[i], color, bg);
        cur_x += 6;
    }
}

static void draw_text_centered(int y, const char *text, uint16_t color, uint16_t bg)
{
    if (!text) return;
    int len = strlen(text);
    int x = (LCD_H_RES - len * 6) / 2;
    if (x < 0) x = 0;
    draw_text(x, y, text, color, bg);
}

static void draw_decorative_line(void)
{
    draw_rect(10, 45, LCD_H_RES - 10, 46, ACCENT_COLOR, false);
    for (int i = 10; i < LCD_H_RES - 10; i += 8) {
        draw_pixel(i, 45, ACCENT_COLOR);
        draw_pixel(i, 46, ACCENT_COLOR);
    }
}

static void draw_thinking_animation(int x, int y)
{
    static int frame = 0;
    frame++;

    for (int i = 0; i < 3; i++) {
        int offset = (frame + i * 4) % 12;
        int height = 3 + offset;
        uint16_t color = (i == 1) ? PRIMARY_COLOR : ACCENT_COLOR;
        if (height > 6) height = 12 - height;
        draw_rect(x + i * 8, y + 10 - height, x + i * 8 + 4, y + 10, color, true);
    }
}

static void draw_state_indicator(void)
{
    int cx = LCD_H_RES / 2;

    switch (current_state) {
        case STATE_SLEEPING:
            draw_text_centered(10, "Z z z", DIM_COLOR, BG_COLOR);
            draw_rect(cx - 15, 25, cx + 15, 35, DIM_COLOR, false);
            break;

        case STATE_THINKING:
            draw_text_centered(8, "THINKING", PRIMARY_COLOR, BG_COLOR);
            draw_thinking_animation(cx - 14, 22);
            break;

        case STATE_LISTENING:
            draw_text_centered(8, "LISTENING", ACCENT_COLOR, BG_COLOR);
            draw_rect(cx - 12, 22, cx + 12, 35, ACCENT_COLOR, false);
            draw_rect(cx - 6, 25, cx + 6, 32, ACCENT_COLOR, true);
            break;

        case STATE_SPEAKING:
            draw_text_centered(8, "SPEAKING", 0x00FF, BG_COLOR);
            for (int i = 0; i < 5; i++) {
                int h = 4 + (i % 3) * 4;
                draw_rect(cx - 20 + i * 9, 30 - h, cx - 17 + i * 9, 30, 0x00FF, true);
            }
            break;

        case STATE_ERROR:
            draw_text_centered(8, "ERROR", 0xF800, BG_COLOR);
            draw_rect(cx - 10, 22, cx + 10, 35, 0xF800, false);
            draw_line(cx - 7, 25, cx + 7, 32, 0xF800);
            draw_line(cx + 7, 25, cx - 7, 32, 0xF800);
            break;
    }
}

static void redraw_screen(void)
{
    draw_rect(0, 0, LCD_H_RES, LCD_V_RES, BG_COLOR, true);

    draw_state_indicator();

    draw_decorative_line();

    if (current_message[0] != '\0') {
        int y = 55;
        int max_width = LCD_H_RES - 16;
        int char_width = 6;
        int chars_per_line = max_width / char_width;

        int len = strlen(current_message);
        int pos = 0;

        while (pos < len && y < LCD_V_RES - 10) {
            char buffer[32] = {0};
            int char_count = 0;

            while (pos < len && char_count < chars_per_line) {
                if (current_message[pos] == '\n') {
                    pos++;
                    break;
                }
                buffer[char_count++] = current_message[pos++];
            }
            buffer[char_count] = '\0';

            draw_text(8, y, buffer, SECONDARY_COLOR, BG_COLOR);
            y += 12;
        }
    }
}

esp_err_t lcd_display_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD display (ST7789 240x240)");

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    lcd_backlight_set(true);

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_SCLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_LOGI(TAG, "Initializing SPI bus...");
    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_DC,
        .cs_gpio_num = PIN_CS,
        .pclk_hz = LCD_PIXEL_CLK,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 3,
        .trans_queue_depth = 10,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_LOGI(TAG, "Resetting panel...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(TAG, "Initializing panel...");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(TAG, "Setting display parameters...");
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "LCD initialized successfully");

    redraw_screen();

    return ESP_OK;
}

void lcd_set_state(int state)
{
    if (state < STATE_SLEEPING || state > STATE_ERROR) return;
    current_state = state;
    redraw_screen();
}

void lcd_show_message(const char *message)
{
    if (message) {
        strncpy(current_message, message, sizeof(current_message) - 1);
        current_message[sizeof(current_message) - 1] = '\0';
    } else {
        current_message[0] = '\0';
    }
    redraw_screen();
}
