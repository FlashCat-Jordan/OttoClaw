/**
 * LCD Display Driver for ST7789 240x240 TFT
 * 
 * Default pin configuration:
 * - MOSI: GPIO 23
 * - SCLK: GPIO 18  
 * - CS:   GPIO 5
 * - DC:   GPIO 2
 * - RST:  GPIO 4
 * - BL:   GPIO 15
 */

#include "lcd_display.h"
#include "openclaw.h"

#include <string.h>
#include <stdlib.h>
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

/* Pin definitions - Otto robot with camera version */
#define LCD_HOST       SPI3_HOST
#define LCD_PIXEL_CLK  (40 * 1000 * 1000)  // 40MHz

/* LCD pin configuration for Otto robot (with camera version):
 * MOSI: GPIO 10, CLK: GPIO 9, DC: GPIO 46, RST: GPIO 11, CS: GPIO 12, BL: GPIO 3
 */
#define PIN_SCLK       9    // CLK
#define PIN_MOSI       10   // MOSI
#define PIN_DC         46   // DC (Data/Command)
#define PIN_RST        11   // RST
#define PIN_CS         12   // CS
#define PIN_BL         3    // Backlight

#define LCD_H_RES      240
#define LCD_V_RES      240

static esp_lcd_panel_handle_t panel_handle = NULL;

void lcd_backlight_set(bool on)
{
    gpio_set_level(PIN_BL, on ? 1 : 0);
}

esp_err_t lcd_display_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD display (ST7789 240x240)");

    /* Configure backlight GPIO */
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    
    /* Test: Toggle backlight to verify GPIO control */
    ESP_LOGI(TAG, "Testing backlight GPIO...");
    for (int i = 0; i < 3; i++) {
        lcd_backlight_set(true);
        vTaskDelay(pdMS_TO_TICKS(200));
        lcd_backlight_set(false);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    lcd_backlight_set(false);  // Keep backlight off initially

    /* Configure SPI bus */
    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_SCLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_LOGI(TAG, "Initializing SPI bus on SPI2_HOST...");
    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "SPI bus initialized successfully");

    /* Configure LCD panel IO */
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
    ESP_LOGI(TAG, "Creating panel IO...");
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create panel IO: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Panel IO created successfully");

    /* Install ST7789 panel driver */
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    /* Reset and initialize panel */
    ESP_LOGI(TAG, "Resetting panel...");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(200));  // Increased delay
    
    ESP_LOGI(TAG, "Initializing panel...");
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    vTaskDelay(pdMS_TO_TICKS(200));  // Increased delay
    
    ESP_LOGI(TAG, "Setting color inversion...");
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));  // Try true
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "Setting swap_xy and mirror...");
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, false, false));
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "Setting gap offset (common for 240x240 displays)...");
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));
    vTaskDelay(pdMS_TO_TICKS(50));
    
    ESP_LOGI(TAG, "Turning on display...");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Turn on backlight */
    lcd_backlight_set(true);
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for backlight to stabilize

    ESP_LOGI(TAG, "LCD initialized successfully");

    /* Test: Fill screen with white color first */
    ESP_LOGI(TAG, "Testing screen with solid color...");
    
    // Create a white screen buffer (RGB565: 0xFFFF = white)
    uint16_t *test_buffer = heap_caps_malloc(LCD_H_RES * 80 * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (test_buffer) {
        for (int i = 0; i < LCD_H_RES * 80; i++) {
            test_buffer[i] = 0xFFFF;  // White color
        }
        
        // Fill screen in 3 chunks (80 lines each)
        for (int chunk = 0; chunk < 3; chunk++) {
            ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 
                0, chunk * 80, LCD_H_RES, (chunk + 1) * 80, test_buffer));
        }
        
        free(test_buffer);
        ESP_LOGI(TAG, "White screen displayed");
    }
    
    vTaskDelay(pdMS_TO_TICKS(2000));  // Wait 2 seconds
    
    /* Display OpenClaw logo */
    ESP_LOGI(TAG, "Drawing OpenClaw logo...");
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, OPENCLAW_WIDTH, OPENCLAW_HEIGHT, openclaw_data));
    ESP_LOGI(TAG, "Logo displayed");

    return ESP_OK;
}
