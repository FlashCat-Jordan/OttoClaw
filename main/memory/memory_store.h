#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Initialize memory store. Ensures SPIFFS directories exist.
 */
esp_err_t memory_store_init(void);

/**
 * Read long-term memory (MEMORY.md) into buffer.
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if file missing
 */
esp_err_t memory_read_long_term(char *buf, size_t size);

/**
 * Write content to long-term memory (MEMORY.md).
 */
esp_err_t memory_write_long_term(const char *content);

/**
 * Append a note to today's daily memory file (YYYY-MM-DD.md).
 */
esp_err_t memory_append_today(const char *note);

/**
 * Read recent daily memories (last N days) into buffer.
 * @param days  Number of days to look back (default 3)
 */
esp_err_t memory_read_recent(char *buf, size_t size, int days);
