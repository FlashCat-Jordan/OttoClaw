#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    LCD_STATE_SLEEPING,
    LCD_STATE_THINKING,
    LCD_STATE_SPEAKING,
    LCD_STATE_LISTENING,
    LCD_STATE_CONNECTING,
    LCD_STATE_CONNECTED,
    LCD_STATE_ERROR,
} lcd_state_t;

esp_err_t lcd_display_init(void);

void lcd_backlight_set(bool on);

void lcd_set_state(lcd_state_t state);

void lcd_show_chat_message(const char *role, const char *content);

void lcd_clear_chat(void);

void lcd_set_status_text(const char *text);
