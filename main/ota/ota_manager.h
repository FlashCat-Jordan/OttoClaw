#pragma once

#include "esp_err.h"

/**
 * Perform OTA firmware update from a URL.
 * Downloads the firmware binary and applies it. Reboots on success.
 *
 * @param url  HTTPS URL to the firmware .bin file
 * @return ESP_OK on success (device will reboot), error code otherwise
 */
esp_err_t ota_update_from_url(const char *url);
