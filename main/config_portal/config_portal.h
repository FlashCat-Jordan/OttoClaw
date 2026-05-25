#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the configuration portal in AP mode.
 *
 * - Starts a WiFi Access Point (open, SSID = "OttoClaw-XXXX")
 * - Starts an HTTP server on port 80 serving setup.html and JSON APIs
 * - Shows on-screen setup instructions for http://192.168.4.1
 * - Blocks in a FreeRTOS task until config_portal_stop() is called
 *   or /api/setup/complete is posted.
 *
 * @return ESP_OK if portal started successfully.
 */
esp_err_t config_portal_start(void);

/**
 * @brief Stop the configuration portal and tear down AP mode.
 * Safe to call from any task.
 */
void config_portal_stop(void);

/**
 * @brief Check whether the portal is currently running.
 */
bool config_portal_is_running(void);

#ifdef __cplusplus
}
#endif
