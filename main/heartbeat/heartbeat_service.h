#pragma once

#include "esp_err.h"
#include <stdbool.h>

/**
 * Initialize the heartbeat service.
 *
 * @return ESP_OK on success
 */
esp_err_t heartbeat_service_init(void);

/**
 * Start the heartbeat service.
 *
 * @return ESP_OK on success
 */
esp_err_t heartbeat_service_start(void);

/**
 * Stop the heartbeat service.
 *
 * @return ESP_OK on success
 */
esp_err_t heartbeat_service_stop(void);

/**
 * Check if the heartbeat service is running.
 *
 * @return true if running, false otherwise
 */
bool heartbeat_service_is_running(void);

/**
 * Manually trigger a heartbeat check.
 *
 * @return ESP_OK on success
 */
esp_err_t heartbeat_service_trigger(void);
