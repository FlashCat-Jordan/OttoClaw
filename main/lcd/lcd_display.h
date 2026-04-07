#pragma once

#include <stdbool.h>
#include "esp_err.h"

/**
 * Initialize LCD display and show OpenClaw logo
 */
esp_err_t lcd_display_init(void);

/**
 * Turn LCD backlight on/off
 */
void lcd_backlight_set(bool on);
