#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize DingTalk bot with credentials from NVS or defaults
 */
esp_err_t dingtalk_bot_init(void);

/**
 * @brief Start DingTalk Stream mode task (WebSocket connection to DingTalk)
 */
esp_err_t dingtalk_bot_start(void);

/**
 * @brief Stop DingTalk Stream mode task
 */
esp_err_t dingtalk_bot_stop(void);

/**
 * @brief Send message to DingTalk user
 * @param user_id DingTalk user staffId
 * @param text Message text
 */
esp_err_t dingtalk_send_message(const char *user_id, const char *text);

/**
 * @brief Set DingTalk credentials (saves to NVS)
 * @param app_key AppKey from DingTalk open platform
 * @param app_secret AppSecret from DingTalk open platform
 */
esp_err_t dingtalk_set_credentials(const char *app_key, const char *app_secret);

/**
 * @brief Check if DingTalk Stream is connected
 */
bool dingtalk_is_connected(void);

#ifdef __cplusplus
}
#endif
