#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialize the cron service.
 * Loads CRON.md file and sets up periodic timers.
 *
 * @return ESP_OK on success
 */
esp_err_t cron_service_init(void);

/**
 * Start the cron service.
 *
 * @return ESP_OK on success
 */
esp_err_t cron_service_start(void);

/**
 * Stop the cron service.
 *
 * @return ESP_OK on success
 */
esp_err_t cron_service_stop(void);

/**
 * Check if the cron service is running.
 *
 * @return true if running, false otherwise
 */
bool cron_service_is_running(void);

/**
 * Reload the CRON.md file and update scheduled tasks.
 *
 * @return ESP_OK on success
 */
esp_err_t cron_service_reload(void);
