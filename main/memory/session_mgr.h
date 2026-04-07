#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <time.h>

esp_err_t session_mgr_init(void);

esp_err_t session_append(const char *chat_id, const char *role, const char *content);

esp_err_t session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs);

esp_err_t session_clear(const char *chat_id);

void session_list(void);

esp_err_t session_get_metadata(const char *chat_id, time_t *created_at, time_t *updated_at);

esp_err_t session_get_info_json(const char *chat_id, char *buf, size_t size);
