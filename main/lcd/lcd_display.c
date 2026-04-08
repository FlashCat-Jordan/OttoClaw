#include "lcd_display.h"

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

LV_FONT_DECLARE(font_chinese_14);
LV_FONT_DECLARE(font_chinese_12);

static const char *TAG = "lcd";

#define LCD_HOST        SPI3_HOST
#define LCD_PIXEL_CLK   (40 * 1000 * 1000)
#define PIN_SCLK        9
#define PIN_MOSI        10
#define PIN_DC          46
#define PIN_RST         11
#define PIN_CS          12
#define PIN_BL          3
#define LCD_H_RES       240
#define LCD_V_RES       240

#define MAX_CHAT_MSGS   20

#define COLOR_BG            lv_color_hex(0x0A0A1A)
#define COLOR_TOP_BAR       lv_color_hex(0x101028)
#define COLOR_STATUS_BG     lv_color_hex(0x181838)
#define COLOR_USER_BUBBLE   lv_color_hex(0x1A5C3A)
#define COLOR_ASSIST_BUBBLE lv_color_hex(0x1E1E3A)
#define COLOR_TEXT           lv_color_hex(0xE0E0F0)
#define COLOR_TEXT_DIM       lv_color_hex(0x707090)
#define COLOR_ACCENT         lv_color_hex(0x00B4D8)
#define COLOR_ACCENT2        lv_color_hex(0x7B2FF7)
#define COLOR_ERROR          lv_color_hex(0xE04040)
#define COLOR_BORDER         lv_color_hex(0x2A2A4A)
#define COLOR_SLEEP_ICON     lv_color_hex(0x4040A0)

static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;
static lv_display_t *lvgl_disp = NULL;

static lv_obj_t *screen = NULL;
static lv_obj_t *top_bar = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *state_icon_label = NULL;
static lv_obj_t *chat_container = NULL;
static lv_obj_t *bottom_bar = NULL;
static lv_obj_t *bottom_label = NULL;
static lv_obj_t *state_indicator = NULL;

static lcd_state_t current_state = LCD_STATE_SLEEPING;
static int msg_count = 0;

static const lv_font_t *font_text = NULL;
static const lv_font_t *font_small = NULL;
static const lv_font_t *font_icon = NULL;

void lcd_backlight_set(bool on)
{
    gpio_set_level(PIN_BL, on ? 1 : 0);
}

static void setup_ui(void)
{
    font_text = &font_chinese_14;
    font_small = &font_chinese_12;
    font_icon = &lv_font_montserrat_16;

    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    top_bar = lv_obj_create(screen);
    lv_obj_set_size(top_bar, LCD_H_RES, 28);
    lv_obj_align(top_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(top_bar, COLOR_TOP_BAR, 0);
    lv_obj_set_style_bg_opa(top_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(top_bar, 0, 0);
    lv_obj_set_style_border_width(top_bar, 0, 0);
    lv_obj_set_style_pad_all(top_bar, 0, 0);
    lv_obj_set_style_pad_left(top_bar, 8, 0);
    lv_obj_set_style_pad_right(top_bar, 8, 0);
    lv_obj_set_flex_flow(top_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(top_bar, LV_SCROLLBAR_MODE_OFF);

    state_icon_label = lv_label_create(top_bar);
    lv_obj_set_style_text_font(state_icon_label, font_icon, 0);
    lv_obj_set_style_text_color(state_icon_label, COLOR_ACCENT, 0);
    lv_label_set_text(state_icon_label, LV_SYMBOL_WIFI);

    status_label = lv_label_create(top_bar);
    lv_obj_set_style_text_font(status_label, font_small, 0);
    lv_obj_set_style_text_color(status_label, COLOR_TEXT_DIM, 0);
    lv_label_set_text(status_label, "Initializing...");
    lv_obj_set_flex_grow(status_label, 1);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_SCROLL_CIRCULAR);

    state_indicator = lv_obj_create(top_bar);
    lv_obj_set_size(state_indicator, 8, 8);
    lv_obj_set_style_bg_color(state_indicator, COLOR_ACCENT, 0);
    lv_obj_set_style_bg_opa(state_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(state_indicator, 4, 0);
    lv_obj_set_style_border_width(state_indicator, 0, 0);
    lv_obj_set_style_pad_all(state_indicator, 0, 0);

    chat_container = lv_obj_create(screen);
    lv_obj_set_size(chat_container, LCD_H_RES, LCD_V_RES - 28 - 24);
    lv_obj_align(chat_container, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_style_bg_color(chat_container, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(chat_container, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(chat_container, 0, 0);
    lv_obj_set_style_border_width(chat_container, 0, 0);
    lv_obj_set_style_pad_all(chat_container, 4, 0);
    lv_obj_set_style_pad_top(chat_container, 6, 0);
    lv_obj_set_flex_flow(chat_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(chat_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(chat_container, 4, 0);
    lv_obj_set_scrollbar_mode(chat_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(chat_container, LV_DIR_VER);

    bottom_bar = lv_obj_create(screen);
    lv_obj_set_size(bottom_bar, LCD_H_RES, 24);
    lv_obj_align(bottom_bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(bottom_bar, COLOR_STATUS_BG, 0);
    lv_obj_set_style_bg_opa(bottom_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(bottom_bar, 0, 0);
    lv_obj_set_style_border_width(bottom_bar, 0, 0);
    lv_obj_set_style_border_side(bottom_bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bottom_bar, COLOR_BORDER, 0);
    lv_obj_set_style_pad_all(bottom_bar, 0, 0);
    lv_obj_set_scrollbar_mode(bottom_bar, LV_SCROLLBAR_MODE_OFF);

    bottom_label = lv_label_create(bottom_bar);
    lv_obj_set_style_text_font(bottom_label, font_small, 0);
    lv_obj_set_style_text_color(bottom_label, COLOR_TEXT_DIM, 0);
    lv_label_set_text(bottom_label, "");
    lv_obj_set_width(bottom_label, LCD_H_RES - 12);
    lv_label_set_long_mode(bottom_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_align(bottom_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(bottom_bar, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *create_chat_bubble(const char *role, const char *content)
{
    bool is_user = (strcmp(role, "user") == 0);
    bool is_system = (strcmp(role, "system") == 0);

    lv_obj_t *row = lv_obj_create(chat_container);
    lv_obj_set_width(row, LCD_H_RES - 8);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_scrollbar_mode(row, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *bubble = lv_obj_create(row);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(bubble, is_system ? 4 : 10, 0);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_style_pad_all(bubble, 6, 0);
    lv_obj_set_scrollbar_mode(bubble, LV_SCROLLBAR_MODE_OFF);

    if (is_user) {
        lv_obj_set_style_bg_color(bubble, COLOR_USER_BUBBLE, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_90, 0);
        lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, 0, 0);
        lv_obj_set_width(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_max_width(bubble, (LCD_H_RES * 80) / 100, 0);
    } else if (is_system) {
        lv_obj_set_style_bg_color(bubble, COLOR_BORDER, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_50, 0);
        lv_obj_align(bubble, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_width(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_max_width(bubble, (LCD_H_RES * 90) / 100, 0);
    } else {
        lv_obj_set_style_bg_color(bubble, COLOR_ASSIST_BUBBLE, 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_90, 0);
        lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 0, 0);
        lv_obj_set_width(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_max_width(bubble, (LCD_H_RES * 80) / 100, 0);
    }

    lv_obj_t *txt = lv_label_create(bubble);
    lv_obj_set_style_text_font(txt, font_text, 0);
    lv_obj_set_style_text_color(txt, COLOR_TEXT, 0);
    lv_label_set_text(txt, content);
    lv_obj_set_width(txt, LV_SIZE_CONTENT);
    lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_max_width(txt, (LCD_H_RES * 75) / 100, 0);

    return row;
}

static void anim_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)var, (lv_opa_t)v, 0);
}

static void update_state_display(lcd_state_t state)
{
    if (!state_icon_label || !status_label || !state_indicator) return;

    const char *icon = LV_SYMBOL_WIFI;
    const char *text = "";
    lv_color_t dot_color = COLOR_ACCENT;

    switch (state) {
    case LCD_STATE_SLEEPING:
        icon = LV_SYMBOL_WIFI;
        text = "Sleeping";
        dot_color = COLOR_TEXT_DIM;
        break;
    case LCD_STATE_THINKING:
        icon = LV_SYMBOL_REFRESH;
        text = "Thinking...";
        dot_color = COLOR_ACCENT2;
        break;
    case LCD_STATE_SPEAKING:
        icon = LV_SYMBOL_VOLUME_MAX;
        text = "Speaking";
        dot_color = COLOR_ACCENT;
        break;
    case LCD_STATE_LISTENING:
        icon = LV_SYMBOL_AUDIO;
        text = "Listening";
        dot_color = COLOR_ACCENT;
        break;
    case LCD_STATE_CONNECTING:
        icon = LV_SYMBOL_WIFI;
        text = "Connecting...";
        dot_color = COLOR_ACCENT2;
        break;
    case LCD_STATE_CONNECTED:
        icon = LV_SYMBOL_WIFI;
        text = "Connected";
        dot_color = COLOR_ACCENT;
        break;
    case LCD_STATE_ERROR:
        icon = LV_SYMBOL_WARNING;
        text = "Error";
        dot_color = COLOR_ERROR;
        break;
    }

    lv_label_set_text(state_icon_label, icon);
    lv_label_set_text(status_label, text);
    lv_obj_set_style_bg_color(state_indicator, dot_color, 0);

    if (state == LCD_STATE_THINKING) {
        static lv_anim_t anim;
        lv_anim_init(&anim);
        lv_anim_set_var(&anim, state_indicator);
        lv_anim_set_values(&anim, 80, 255);
        lv_anim_set_time(&anim, 800);
        lv_anim_set_playback_time(&anim, 800);
        lv_anim_set_repeat_count(&anim, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_exec_cb(&anim, anim_opa_cb);
        lv_anim_start(&anim);
    } else {
        lv_anim_delete(state_indicator, NULL);
        lv_obj_set_style_bg_opa(state_indicator, LV_OPA_COVER, 0);
    }
}

esp_err_t lcd_display_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD with LVGL (ST7789 240x240)");

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    lcd_backlight_set(false);

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_SCLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_LOGI(TAG, "Initializing SPI bus...");
    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_DC,
        .cs_gpio_num = PIN_CS,
        .pclk_hz = LCD_PIXEL_CLK,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 3,
        .trans_queue_depth = 10,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &lcd_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel IO failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd_io, &panel_config, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd_panel, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();

    const lvgl_port_cfg_t port_cfg = {
        .task_priority = 4,
        .task_stack = 7168,
        .task_affinity = 1,
        .task_max_sleep_ms = 500,
        .timer_period_ms = 5,
    };
    ret = lvgl_port_init(&port_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL port init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .control_handle = NULL,
        .buffer_size = LCD_H_RES * 20,
        .double_buffer = false,
        .trans_size = 0,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    if (lvgl_disp == NULL) {
        ESP_LOGE(TAG, "Failed to add LVGL display");
        return ESP_FAIL;
    }

    lcd_backlight_set(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Setting up UI...");
    if (lvgl_port_lock(0)) {
        setup_ui();
        update_state_display(LCD_STATE_SLEEPING);
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "LCD+LVGL initialized successfully");
    return ESP_OK;
}

void lcd_set_state(lcd_state_t state)
{
    current_state = state;
    if (lvgl_port_lock(500)) {
        update_state_display(state);
        lvgl_port_unlock();
    }
}

void lcd_show_chat_message(const char *role, const char *content)
{
    if (!content || content[0] == '\0') return;

    if (lvgl_port_lock(500)) {
        uint32_t child_cnt = lv_obj_get_child_cnt(chat_container);
        if (child_cnt >= MAX_CHAT_MSGS) {
            lv_obj_t *first = lv_obj_get_child(chat_container, 0);
            if (first) lv_obj_del(first);
        }

        lv_obj_t *bubble_row = create_chat_bubble(role, content);
        if (bubble_row) {
            lv_obj_scroll_to_view_recursive(bubble_row, LV_ANIM_ON);
        }
        msg_count++;

        if (bottom_bar) {
            lv_obj_remove_flag(bottom_bar, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(bottom_label, content);
        }

        lvgl_port_unlock();
    }
}

void lcd_clear_chat(void)
{
    if (lvgl_port_lock(500)) {
        lv_obj_clean(chat_container);
        msg_count = 0;
        if (bottom_bar) {
            lv_obj_add_flag(bottom_bar, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(bottom_label, "");
        }
        lvgl_port_unlock();
    }
}

void lcd_set_status_text(const char *text)
{
    if (lvgl_port_lock(500)) {
        if (status_label) {
            lv_label_set_text(status_label, text ? text : "");
        }
        lvgl_port_unlock();
    }
}
