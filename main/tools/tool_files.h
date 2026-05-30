#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Read a file from SPIFFS.
 * Input JSON: {"path": "/spiffs/..."}
 */
esp_err_t tool_read_file_execute(const char *input_json, char *output, size_t output_size);

/**
 * Write/overwrite a file on SPIFFS.
 * Input JSON: {"path": "/spiffs/...", "content": "..."}
 */
esp_err_t tool_write_file_execute(const char *input_json, char *output, size_t output_size);

/**
 * Find-and-replace edit a file on SPIFFS.
 * Input JSON: {"path": "/spiffs/...", "old_string": "...", "new_string": "..."}
 */
esp_err_t tool_edit_file_execute(const char *input_json, char *output, size_t output_size);

/**
 * List files on SPIFFS, optionally filtered by path prefix.
 * Input JSON: {"prefix": "/spiffs/..."} (prefix is optional)
 */
esp_err_t tool_list_dir_execute(const char *input_json, char *output, size_t output_size);
