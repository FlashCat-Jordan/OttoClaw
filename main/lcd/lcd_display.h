#pragma once

#include <stdbool.h>
#include "esp_err.h"

#define STATE_SLEEPING  0
#define STATE_THINKING  1
#define STATE_LISTENING 2
#define STATE_SPEAKING  3
#define STATE_ERROR     4

esp_err_t lcd_display_init(void);

void lcd_backlight_set(bool on);

void lcd_set_state(int state);

void lcd_show_message(const char *message);
