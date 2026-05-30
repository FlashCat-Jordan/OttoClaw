#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Initialize the skills system.
 * Loads all SKILL.md files from /spiffs/skills/ directory.
 *
 * @return ESP_OK on success
 */
esp_err_t skills_init(void);

/**
 * Get the combined content of all loaded skills.
 *
 * @param buf   Output buffer
 * @param size  Buffer size
 * @return ESP_OK on success
 */
esp_err_t skills_get_content(char *buf, size_t size);

/**
 * Get the number of loaded skills.
 *
 * @return Number of skills
 */
int skills_get_count(void);
