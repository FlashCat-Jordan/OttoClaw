#include "dingtalk_bot.h"
#include "mimi_config.h"
#include "bus/message_bus.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_transport.h"
#include "esp_transport_ssl.h"
#include "esp_transport_ws.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <sys/time.h>
#include <string.h>

static const char *TAG = "dingtalk";

static char s_app_key[64] = MIMI_SECRET_DINGTALK_APP_KEY;
static char s_app_secret[128] = MIMI_SECRET_DINGTALK_APP_SECRET;
static char s_access_token[2048] = {0};
static int64_t s_token_expire_time = 0;

static esp_transport_list_handle_t s_transport_list = NULL;
static esp_transport_handle_t s_ws_transport = NULL;
static TaskHandle_t s_stream_task = NULL;
static volatile bool s_running = false;
static SemaphoreHandle_t s_send_mutex = NULL;

#define DINGTALK_STREAM_BUF_SIZE  (4 * 1024)
#define DINGTACK_API_HOST "api.dingtalk.com"
#define DINGTALK_STREAM_TOPIC_BOT "/v1.0/im/bot/messages/get"
#define DINGTALK_STREAM_TOPIC_EVENT "*"
#define DINGTALK_MAX_WEBHOOKS  8

typedef struct {
    char endpoint[256];
    char ticket[256];
} dingtalk_stream_credential_t;

typedef struct {
    char user_id[64];
    char webhook[512];
    int64_t expire_time;
} dingtalk_webhook_entry_t;

static dingtalk_webhook_entry_t s_webhooks[DINGTALK_MAX_WEBHOOKS];
static SemaphoreHandle_t s_webhook_mutex = NULL;

#define DINGTALK_MSG_CACHE_SIZE  32
typedef struct {
    char user_id[64];
    char content[256];
    int64_t timestamp;
} dingtalk_msg_cache_entry_t;

static dingtalk_msg_cache_entry_t s_msg_cache[DINGTALK_MSG_CACHE_SIZE];
static SemaphoreHandle_t s_msg_cache_mutex = NULL;
static int s_msg_cache_idx = 0;

static esp_err_t dingtalk_refresh_token(void);
static esp_err_t dingtalk_ensure_token(void);
static esp_err_t dingtalk_get_stream_credential(dingtalk_stream_credential_t *cred);
static void dingtalk_stream_task(void *arg);
static esp_err_t dingtalk_handle_message(const char *data, int len);
static esp_err_t dingtalk_send_ack(const char *message_id, int code, const char *message);
static esp_err_t dingtalk_reply_to_message(const char *session_webhook, const char *text);

static void dingtalk_save_webhook(const char *user_id, const char *webhook)
{
    if (!user_id || !webhook) return;
    
    if (xSemaphoreTake(s_webhook_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire webhook mutex");
        return;
    }
    
    int64_t now = esp_timer_get_time() / 1000;
    
    int slot = -1;
    for (int i = 0; i < DINGTALK_MAX_WEBHOOKS; i++) {
        if (s_webhooks[i].user_id[0] == '\0') {
            if (slot < 0) slot = i;
        } else if (strcmp(s_webhooks[i].user_id, user_id) == 0) {
            slot = i;
            break;
        } else if (s_webhooks[i].expire_time < now) {
            if (slot < 0) slot = i;
        }
    }
    
    if (slot >= 0) {
        strncpy(s_webhooks[slot].user_id, user_id, sizeof(s_webhooks[slot].user_id) - 1);
        strncpy(s_webhooks[slot].webhook, webhook, sizeof(s_webhooks[slot].webhook) - 1);
        s_webhooks[slot].expire_time = now + 3600000;
        ESP_LOGD(TAG, "Saved webhook for %s", user_id);
    } else {
        ESP_LOGW(TAG, "No webhook slot available for %s", user_id);
    }
    
    xSemaphoreGive(s_webhook_mutex);
}

static const char *dingtalk_get_webhook(const char *user_id)
{
    if (!user_id) return NULL;
    
    if (xSemaphoreTake(s_webhook_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return NULL;
    }
    
    const char *result = NULL;
    int64_t now = esp_timer_get_time() / 1000;
    
    for (int i = 0; i < DINGTALK_MAX_WEBHOOKS; i++) {
        if (strcmp(s_webhooks[i].user_id, user_id) == 0 && 
            s_webhooks[i].expire_time > now) {
            result = s_webhooks[i].webhook;
            break;
        }
    }
    
    xSemaphoreGive(s_webhook_mutex);
    return result;
}

static bool dingtalk_is_duplicate_message(const char *user_id, const char *content)
{
    if (!user_id || !content) return false;
    
    if (xSemaphoreTake(s_msg_cache_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire msg cache mutex");
        return false;
    }
    
    int64_t now = esp_timer_get_time() / 1000;
    int64_t expire_time = now - 300000;
    
    for (int i = 0; i < DINGTALK_MSG_CACHE_SIZE; i++) {
        if (s_msg_cache[i].user_id[0] != '\0') {
            if (strcmp(s_msg_cache[i].user_id, user_id) == 0 && 
                strcmp(s_msg_cache[i].content, content) == 0) {
                if (s_msg_cache[i].timestamp > expire_time) {
                    xSemaphoreGive(s_msg_cache_mutex);
                    ESP_LOGD(TAG, "Duplicate message detected: user=%s, content=%s", user_id, content);
                    return true;
                }
            }
        }
    }
    
    s_msg_cache_idx = (s_msg_cache_idx + 1) % DINGTALK_MSG_CACHE_SIZE;
    strncpy(s_msg_cache[s_msg_cache_idx].user_id, user_id, sizeof(s_msg_cache[s_msg_cache_idx].user_id) - 1);
    strncpy(s_msg_cache[s_msg_cache_idx].content, content, sizeof(s_msg_cache[s_msg_cache_idx].content) - 1);
    s_msg_cache[s_msg_cache_idx].timestamp = now;
    
    xSemaphoreGive(s_msg_cache_mutex);
    return false;
}

static void dingtalk_clear_msg_cache(void)
{
    if (xSemaphoreTake(s_msg_cache_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return;
    }
    
    memset(s_msg_cache, 0, sizeof(s_msg_cache));
    s_msg_cache_idx = 0;
    
    xSemaphoreGive(s_msg_cache_mutex);
}

static esp_err_t dingtalk_refresh_token(void)
{
    ESP_LOGI(TAG, "Refreshing access_token...");
    
    char url[256];
    snprintf(url, sizeof(url), 
             "https://oapi.dingtalk.com/gettoken?appkey=%s&appsecret=%s",
             s_app_key, s_app_secret);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .disable_auto_redirect = false,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    char *buffer = malloc(2048);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        free(buffer);
        return ESP_FAIL;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buffer);
        return err;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "HTTP status: %d, content_length: %d", status, content_length);
    
    if (status != 200) {
        ESP_LOGE(TAG, "HTTP error: status=%d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(buffer);
        return ESP_FAIL;
    }
    
    int read_len = 0;
    if (content_length > 0 && content_length < 2048) {
        read_len = esp_http_client_read(client, buffer, content_length);
    } else {
        read_len = esp_http_client_read(client, buffer, 2047);
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    
    if (read_len <= 0) {
        ESP_LOGE(TAG, "Failed to read response: %d", read_len);
        free(buffer);
        return ESP_FAIL;
    }
    buffer[read_len] = '\0';
    
    ESP_LOGD(TAG, "Token response: %s", buffer);
    
    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }
    
    cJSON *errcode = cJSON_GetObjectItem(root, "errcode");
    if (errcode && errcode->valueint != 0) {
        cJSON *errmsg = cJSON_GetObjectItem(root, "errmsg");
        ESP_LOGE(TAG, "DingTalk API error: %d - %s", 
                 errcode->valueint, 
                 errmsg ? errmsg->valuestring : "unknown");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON *token = cJSON_GetObjectItem(root, "access_token");
    cJSON *expires = cJSON_GetObjectItem(root, "expires_in");
    
    if (!token || !token->valuestring || !expires) {
        ESP_LOGE(TAG, "Missing access_token or expires_in in response");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    strncpy(s_access_token, token->valuestring, sizeof(s_access_token) - 1);
    s_token_expire_time = (esp_timer_get_time() / 1000000 + expires->valueint - 300);
    
    ESP_LOGI(TAG, "Token refreshed, expires in %d seconds", expires->valueint);
    
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t dingtalk_ensure_token(void)
{
    int64_t now = esp_timer_get_time() / 1000000;
    
    if (s_access_token[0] == '\0' || now >= s_token_expire_time) {
        return dingtalk_refresh_token();
    }
    
    return ESP_OK;
}

static esp_err_t dingtalk_get_stream_credential(dingtalk_stream_credential_t *cred)
{
    if (!cred) return ESP_ERR_INVALID_ARG;
    
    if (dingtalk_ensure_token() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to ensure token for Stream credential");
        return ESP_FAIL;
    }
    
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "clientId", s_app_key);
    cJSON_AddStringToObject(body, "clientSecret", s_app_secret);
    cJSON_AddStringToObject(body, "localIp", "0.0.0.0");
    
    cJSON *subs = cJSON_CreateArray();
    
    cJSON *sub1 = cJSON_CreateObject();
    cJSON_AddStringToObject(sub1, "topic", DINGTALK_STREAM_TOPIC_BOT);
    cJSON_AddStringToObject(sub1, "type", "CALLBACK");
    cJSON_AddItemToArray(subs, sub1);
    
    cJSON *sub2 = cJSON_CreateObject();
    cJSON_AddStringToObject(sub2, "topic", DINGTALK_STREAM_TOPIC_EVENT);
    cJSON_AddStringToObject(sub2, "type", "EVENT");
    cJSON_AddItemToArray(subs, sub2);
    
    cJSON_AddItemToObject(body, "subscriptions", subs);
    cJSON_AddStringToObject(body, "ua", "mimiclaw-esp32/1.0");
    
    char *json_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to create JSON body");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "Stream credential request body: %s", json_str);
    
    char url[128];
    snprintf(url, sizeof(url), "https://" DINGTACK_API_HOST "/v1.0/gateway/connections/open");
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .disable_auto_redirect = true,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(json_str);
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept", "application/json");
    esp_http_client_set_header(client, "User-Agent", "mimiclaw-esp32/1.0");
    
    ESP_LOGI(TAG, "Sending HTTP POST to: %s", url);
    ESP_LOGI(TAG, "Request body length: %d", strlen(json_str));
    
    esp_err_t err = esp_http_client_open(client, strlen(json_str));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(json_str);
        return err;
    }
    
    int written = esp_http_client_write(client, json_str, strlen(json_str));
    ESP_LOGI(TAG, "Written %d bytes to HTTP client", written);
    
    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "Stream credential HTTP status: %d, len: %d", status, content_length);
    
    char *buffer = malloc(2048);
    if (!buffer) {
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        free(json_str);
        return ESP_ERR_NO_MEM;
    }
    
    int read_len = esp_http_client_read(client, buffer, 2047);
    buffer[read_len > 0 ? read_len : 0] = '\0';
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(json_str);
    
    if (status != 200 || read_len <= 0) {
        ESP_LOGE(TAG, "Failed to get Stream credential: status=%d, response=%s", status, buffer);
        free(buffer);
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Stream credential response: %s", buffer);
    
    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse Stream credential response");
        return ESP_FAIL;
    }
    
    cJSON *endpoint = cJSON_GetObjectItem(root, "endpoint");
    cJSON *ticket = cJSON_GetObjectItem(root, "ticket");
    
    if (!endpoint || !endpoint->valuestring || !ticket || !ticket->valuestring) {
        ESP_LOGE(TAG, "Missing endpoint or ticket in Stream credential response");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    strncpy(cred->endpoint, endpoint->valuestring, sizeof(cred->endpoint) - 1);
    strncpy(cred->ticket, ticket->valuestring, sizeof(cred->ticket) - 1);
    
    ESP_LOGI(TAG, "Got Stream credential: endpoint=%s, ticket=%.8s...", 
             cred->endpoint, cred->ticket);
    
    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t dingtalk_parse_websocket_url(const char *url, char *host, int host_size, char *path, int path_size)
{
    if (!url || !host || !path) return ESP_ERR_INVALID_ARG;
    
    const char *prefix = "wss://";
    if (strncmp(url, prefix, strlen(prefix)) != 0) {
        ESP_LOGE(TAG, "Invalid WebSocket URL: %s", url);
        return ESP_FAIL;
    }
    
    const char *host_start = url + strlen(prefix);
    const char *path_start = strchr(host_start, '/');
    
    if (!path_start) {
        strncpy(host, host_start, host_size - 1);
        strcpy(path, "/");
    } else {
        int host_len = path_start - host_start;
        if (host_len >= host_size) host_len = host_size - 1;
        strncpy(host, host_start, host_len);
        host[host_len] = '\0';
        
        strncpy(path, path_start, path_size - 1);
    }
    
    char *port_sep = strchr(host, ':');
    if (port_sep) *port_sep = '\0';
    
    ESP_LOGD(TAG, "Parsed WebSocket URL: host=%s, path=%s", host, path);
    return ESP_OK;
}

static esp_err_t dingtalk_send_ws_message(const char *data, int len)
{
    if (!s_ws_transport || !s_running) {
        ESP_LOGW(TAG, "WebSocket not connected");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(s_send_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to acquire send mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    int written = esp_transport_write(s_ws_transport, data, len, 5000);
    xSemaphoreGive(s_send_mutex);
    
    if (written < 0) {
        ESP_LOGE(TAG, "Failed to send WebSocket message");
        return ESP_FAIL;
    }
    
    ESP_LOGD(TAG, "Sent %d bytes via WebSocket", written);
    return ESP_OK;
}

static esp_err_t dingtalk_send_ack(const char *message_id, int code, const char *message)
{
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddNumberToObject(resp, "code", code);
    cJSON_AddStringToObject(resp, "message", message ? message : "OK");
    
    cJSON *headers = cJSON_CreateObject();
    cJSON_AddStringToObject(headers, "messageId", message_id);
    cJSON_AddStringToObject(headers, "contentType", "application/json");
    cJSON_AddItemToObject(resp, "headers", headers);
    
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNullToObject(data, "response");
    char *data_str = cJSON_PrintUnformatted(data);
    cJSON_Delete(data);
    cJSON_AddStringToObject(resp, "data", data_str ? data_str : "{}");
    if (data_str) free(data_str);
    
    char *json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);
    
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to create ACK JSON");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGD(TAG, "Sending ACK: %s", json_str);
    
    esp_err_t err = dingtalk_send_ws_message(json_str, strlen(json_str));
    free(json_str);
    
    return err;
}

static esp_err_t dingtalk_reply_to_message(const char *session_webhook, const char *text)
{
    if (!session_webhook || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Replying via session webhook: %s", session_webhook);
    
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "msgtype", "text");
    
    cJSON *text_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(text_obj, "content", text);
    cJSON_AddItemToObject(body, "text", text_obj);
    
    char *json_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);
    
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to create reply JSON");
        return ESP_ERR_NO_MEM;
    }
    
    esp_http_client_config_t config = {
        .url = session_webhook,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        free(json_str);
        ESP_LOGE(TAG, "Failed to init HTTP client for reply");
        return ESP_FAIL;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));
    
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    
    free(json_str);
    esp_http_client_cleanup(client);
    
    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "Failed to send reply: %s, status=%d", esp_err_to_name(err), status);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Reply sent successfully");
    return ESP_OK;
}

static esp_err_t dingtalk_handle_bot_message(cJSON *data)
{
    if (!data) return ESP_ERR_INVALID_ARG;
    
    cJSON *text = cJSON_GetObjectItem(data, "text");
    cJSON *content = text ? cJSON_GetObjectItem(text, "content") : NULL;
    cJSON *sender_id = cJSON_GetObjectItem(data, "senderStaffId");
    cJSON *sender_nick = cJSON_GetObjectItem(data, "senderNick");
    cJSON *session_webhook = cJSON_GetObjectItem(data, "sessionWebhook");
    cJSON *conversation_type = cJSON_GetObjectItem(data, "conversationType");
    cJSON *is_in_at_list = cJSON_GetObjectItem(data, "isInAtList");
    cJSON *conversation_title = cJSON_GetObjectItem(data, "conversationTitle");
    
    if (!content || !content->valuestring) {
        ESP_LOGW(TAG, "No content in bot message");
        return ESP_FAIL;
    }
    
    const char *msg_content = content->valuestring;
    const char *user_id = sender_id ? sender_id->valuestring : "unknown";
    const char *nick = sender_nick ? sender_nick->valuestring : "User";
    const char *webhook = session_webhook ? session_webhook->valuestring : NULL;
    int conv_type = conversation_type ? conversation_type->valueint : 1;
    bool is_at = is_in_at_list && is_in_at_list->type == cJSON_True;
    const char *conv_title = conversation_title ? conversation_title->valuestring : "";
    
    ESP_LOGI(TAG, "Bot message from %s (%s): %s", nick, user_id, msg_content);
    ESP_LOGI(TAG, "Conversation type: %d, is_at: %d, title: %s", conv_type, is_at, conv_title);
    
    if (conv_type == 2 && !is_at) {
        ESP_LOGD(TAG, "Group message without @, ignoring");
        return ESP_OK;
    }
    
    if (webhook) {
        dingtalk_save_webhook(user_id, webhook);
    }
    
    char *msg_copy = strdup(msg_content);
    if (!msg_copy) return ESP_ERR_NO_MEM;
    
    char chat_id[64];
    snprintf(chat_id, sizeof(chat_id), "dt_%s", user_id);
    
    mimi_msg_t msg = {0};
    strncpy(msg.channel, MIMI_CHAN_DINGTALK, sizeof(msg.channel) - 1);
    strncpy(msg.chat_id, chat_id, sizeof(msg.chat_id) - 1);
    msg.content = msg_copy;
    
    esp_err_t err = message_bus_push_inbound(&msg);
    if (err != ESP_OK) {
        free(msg_copy);
        ESP_LOGE(TAG, "Failed to push message to bus");
        return err;
    }
    
    ESP_LOGI(TAG, "Message pushed to agent bus: %s", chat_id);
    return ESP_OK;
}

static esp_err_t dingtalk_handle_message(const char *data, int len)
{
    if (!data || len <= 0) return ESP_ERR_INVALID_ARG;
    
    ESP_LOGD(TAG, "Handling message: %.*s", len > 200 ? 200 : len, data);
    
    cJSON *root = cJSON_ParseWithLength(data, len);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse message JSON");
        return ESP_FAIL;
    }
    
    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *headers = cJSON_GetObjectItem(root, "headers");
    cJSON *msg_data = cJSON_GetObjectItem(root, "data");
    
    if (!type || !type->valuestring) {
        ESP_LOGW(TAG, "Missing type in message");
        cJSON_Delete(root);
        return ESP_FAIL;
    }
    
    cJSON *message_id = headers ? cJSON_GetObjectItem(headers, "messageId") : NULL;
    cJSON *topic = headers ? cJSON_GetObjectItem(headers, "topic") : NULL;
    const char *msg_id_str = message_id ? message_id->valuestring : "";
    const char *topic_str = topic ? topic->valuestring : "";
    
    ESP_LOGI(TAG, "Received message type=%s, topic=%s, msgId=%s", type->valuestring, topic_str, msg_id_str);
    
    if (strcmp(type->valuestring, "SYSTEM") == 0) {
        if (strcmp(topic_str, "ping") == 0) {
            ESP_LOGD(TAG, "Received ping, sending pong");
            cJSON *ping_data = cJSON_Parse(msg_data ? msg_data->valuestring : "{}");
            cJSON *opaque = ping_data ? cJSON_GetObjectItem(ping_data, "opaque") : NULL;
            const char *opaque_val = opaque ? opaque->valuestring : "";
            
            cJSON *resp = cJSON_CreateObject();
            cJSON_AddNumberToObject(resp, "code", 200);
            cJSON_AddStringToObject(resp, "message", "OK");
            
            cJSON *resp_headers = cJSON_CreateObject();
            cJSON_AddStringToObject(resp_headers, "messageId", msg_id_str);
            cJSON_AddStringToObject(resp_headers, "contentType", "application/json");
            cJSON_AddItemToObject(resp, "headers", resp_headers);
            
            cJSON *resp_data = cJSON_CreateObject();
            cJSON_AddStringToObject(resp_data, "opaque", opaque_val);
            char *data_str = cJSON_PrintUnformatted(resp_data);
            cJSON_Delete(resp_data);
            cJSON_AddStringToObject(resp, "data", data_str ? data_str : "{}");
            if (data_str) free(data_str);
            
            if (ping_data) cJSON_Delete(ping_data);
            
            char *json_str = cJSON_PrintUnformatted(resp);
            cJSON_Delete(resp);
            
            if (json_str) {
                dingtalk_send_ws_message(json_str, strlen(json_str));
                free(json_str);
            }
        } else if (strcmp(topic_str, "disconnect") == 0) {
            ESP_LOGI(TAG, "Received disconnect notification");
            cJSON_Delete(root);
            return ESP_ERR_INVALID_STATE;
        }
    } else if (strcmp(type->valuestring, "CALLBACK") == 0) {
        if (strcmp(topic_str, DINGTALK_STREAM_TOPIC_BOT) == 0) {
            dingtalk_send_ack(msg_id_str, 200, "OK");
            
            if (msg_data && msg_data->valuestring) {
                cJSON *bot_data = cJSON_Parse(msg_data->valuestring);
                if (bot_data) {
                    cJSON *text = cJSON_GetObjectItem(bot_data, "text");
                    cJSON *content = text ? cJSON_GetObjectItem(text, "content") : NULL;
                    cJSON *sender_id = cJSON_GetObjectItem(bot_data, "senderStaffId");
                    
                    const char *msg_content = content ? content->valuestring : "";
                    const char *user_id = sender_id ? sender_id->valuestring : "";
                    
                    if (dingtalk_is_duplicate_message(user_id, msg_content)) {
                        ESP_LOGD(TAG, "Skipping duplicate bot message: user=%s, content=%s", user_id, msg_content);
                        cJSON_Delete(bot_data);
                        cJSON_Delete(root);
                        return ESP_OK;
                    }
                    
                    dingtalk_handle_bot_message(bot_data);
                    cJSON_Delete(bot_data);
                }
            }
        } else {
            dingtalk_send_ack(msg_id_str, 200, "OK");
        }
    } else if (strcmp(type->valuestring, "EVENT") == 0) {
        cJSON *resp_data = cJSON_CreateObject();
        cJSON_AddStringToObject(resp_data, "status", "SUCCESS");
        cJSON_AddStringToObject(resp_data, "message", "processed");
        
        cJSON *resp = cJSON_CreateObject();
        cJSON_AddNumberToObject(resp, "code", 200);
        cJSON_AddStringToObject(resp, "message", "OK");
        
        cJSON *resp_headers = cJSON_CreateObject();
        cJSON_AddStringToObject(resp_headers, "messageId", msg_id_str);
        cJSON_AddStringToObject(resp_headers, "contentType", "application/json");
        cJSON_AddItemToObject(resp, "headers", resp_headers);
        char *data_str = cJSON_PrintUnformatted(resp_data);
        cJSON_Delete(resp_data);
        cJSON_AddStringToObject(resp, "data", data_str ? data_str : "{}");
        if (data_str) free(data_str);
        
        char *json_str = cJSON_PrintUnformatted(resp);
        cJSON_Delete(resp);
        
        if (json_str) {
            dingtalk_send_ws_message(json_str, strlen(json_str));
            free(json_str);
        }
        
        ESP_LOGD(TAG, "Received event: %s", topic_str);
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}

static void dingtalk_stream_task(void *arg)
{
    ESP_LOGI(TAG, "Stream task started");
    
    char *recv_buf = malloc(DINGTALK_STREAM_BUF_SIZE);
    if (!recv_buf) {
        ESP_LOGE(TAG, "Failed to allocate receive buffer");
        vTaskDelete(NULL);
        return;
    }
    
    while (s_running) {
        dingtalk_stream_credential_t cred;
        esp_err_t err = dingtalk_get_stream_credential(&cred);
        
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get Stream credential, retrying in 5s...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        char host[128], path[256];
        err = dingtalk_parse_websocket_url(cred.endpoint, host, sizeof(host), path, sizeof(path));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to parse WebSocket URL, retrying in 5s...");
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        char ws_path[768];
        snprintf(ws_path, sizeof(ws_path), "%s?ticket=%s", path, cred.ticket);
        
        ESP_LOGI(TAG, "Connecting to DingTalk Stream: %s%s", host, ws_path);
        
        s_transport_list = esp_transport_list_init();
        esp_transport_handle_t ssl = esp_transport_ssl_init();
        esp_transport_ssl_crt_bundle_attach(ssl, esp_crt_bundle_attach);
        esp_transport_ssl_skip_common_name_check(ssl);
        esp_transport_list_add(s_transport_list, ssl, "ssl");
        
        s_ws_transport = esp_transport_ws_init(ssl);
        esp_transport_ws_set_path(s_ws_transport, ws_path);
        esp_transport_list_add(s_transport_list, s_ws_transport, "ws");
        
        int sock = esp_transport_connect(s_ws_transport, host, 443, 10000);
        
        if (sock < 0) {
            ESP_LOGE(TAG, "WebSocket connection failed: %d", sock);
            esp_transport_list_destroy(s_transport_list);
            s_transport_list = NULL;
            s_ws_transport = NULL;
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        
        ESP_LOGI(TAG, "WebSocket connected to DingTalk Stream!");
        
        while (s_running) {
            int read_len = esp_transport_read(s_ws_transport, recv_buf, DINGTALK_STREAM_BUF_SIZE - 1, 30000);
            
            if (read_len < 0) {
                ESP_LOGW(TAG, "WebSocket read error: %d", read_len);
                break;
            } else if (read_len == 0) {
                ESP_LOGD(TAG, "WebSocket read timeout, checking connection...");
                continue;
            } else if (read_len == -ERR_TCP_TRANSPORT_CONNECTION_CLOSED_BY_FIN) {
                ESP_LOGW(TAG, "WebSocket connection closed by server");
                break;
            }
            
            recv_buf[read_len] = '\0';
            
            ws_transport_opcodes_t opcode = esp_transport_ws_get_read_opcode(s_ws_transport);
            ESP_LOGD(TAG, "Received WebSocket frame: opcode=%d, len=%d", opcode, read_len);
            
            if (opcode == WS_TRANSPORT_OPCODES_TEXT || opcode == WS_TRANSPORT_OPCODES_BINARY) {
                err = dingtalk_handle_message(recv_buf, read_len);
                if (err == ESP_ERR_INVALID_STATE) {
                    ESP_LOGI(TAG, "Disconnect requested by server");
                    break;
                }
            } else if (opcode == WS_TRANSPORT_OPCODES_CLOSE) {
                ESP_LOGI(TAG, "WebSocket close frame received");
                break;
            } else if (opcode == WS_TRANSPORT_OPCODES_PING) {
                ESP_LOGD(TAG, "WebSocket ping received");
            }
        }
        
        ESP_LOGI(TAG, "WebSocket disconnected, cleaning up...");
        esp_transport_close(s_ws_transport);
        esp_transport_list_destroy(s_transport_list);
        s_transport_list = NULL;
        s_ws_transport = NULL;
        
        if (s_running) {
            ESP_LOGI(TAG, "Reconnecting in 3 seconds...");
            vTaskDelay(pdMS_TO_TICKS(3000));
        }
    }
    
    free(recv_buf);
    ESP_LOGI(TAG, "Stream task ended");
    vTaskDelete(NULL);
}

esp_err_t dingtalk_send_message(const char *user_id, const char *text)
{
    if (!user_id || !text) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char *real_user_id = user_id;
    if (strncmp(user_id, "dt_", 3) == 0) {
        real_user_id = user_id + 3;
    }
    
    const char *webhook = dingtalk_get_webhook(real_user_id);
    if (webhook) {
        ESP_LOGI(TAG, "Using session webhook to reply to %s", real_user_id);
        return dingtalk_reply_to_message(webhook, text);
    }
    
    ESP_LOGI(TAG, "No session webhook, using API to send to %s", real_user_id);
    
    if (dingtalk_ensure_token() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get valid access token");
        return ESP_FAIL;
    }
    
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "agent_id", "0");
    cJSON_AddStringToObject(root, "userid_list", real_user_id);
    
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "msgtype", "text");
    
    cJSON *text_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(text_obj, "content", text);
    cJSON_AddItemToObject(msg, "text", text_obj);
    
    cJSON_AddItemToObject(root, "msg", msg);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json_str) {
        ESP_LOGE(TAG, "Failed to create JSON");
        return ESP_ERR_NO_MEM;
    }
    
    char url[2560];
    snprintf(url, sizeof(url),
             "https://oapi.dingtalk.com/topapi/message/corpconversation/asyncsend_v2?access_token=%s",
             s_access_token);
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .disable_auto_redirect = false,
        .cert_pem = NULL,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));
    
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    
    free(json_str);
    esp_http_client_cleanup(client);
    
    if (err != ESP_OK || status != 200) {
        ESP_LOGE(TAG, "Failed to send message: %s, status=%d", 
                 esp_err_to_name(err), status);
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "Message sent to %s", user_id);
    return ESP_OK;
}

esp_err_t dingtalk_bot_init(void)
{
    ESP_LOGI(TAG, "Initializing DingTalk bot...");
    
    s_send_mutex = xSemaphoreCreateMutex();
    if (!s_send_mutex) {
        ESP_LOGE(TAG, "Failed to create send mutex");
        return ESP_ERR_NO_MEM;
    }
    
    s_webhook_mutex = xSemaphoreCreateMutex();
    if (!s_webhook_mutex) {
        ESP_LOGE(TAG, "Failed to create webhook mutex");
        return ESP_ERR_NO_MEM;
    }
    
    s_msg_cache_mutex = xSemaphoreCreateMutex();
    if (!s_msg_cache_mutex) {
        ESP_LOGE(TAG, "Failed to create msg cache mutex");
        return ESP_ERR_NO_MEM;
    }
    
    memset(s_webhooks, 0, sizeof(s_webhooks));
    memset(s_msg_cache, 0, sizeof(s_msg_cache));
    
    nvs_handle_t nvs;
    if (nvs_open(MIMI_NVS_DINGTALK, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len;
        
        len = sizeof(s_app_key);
        nvs_get_str(nvs, MIMI_NVS_KEY_DINGTALK_KEY, s_app_key, &len);
        
        len = sizeof(s_app_secret);
        nvs_get_str(nvs, MIMI_NVS_KEY_DINGTALK_SECRET, s_app_secret, &len);
        
        nvs_close(nvs);
        ESP_LOGI(TAG, "Loaded credentials from NVS");
    }
    
    char masked[32];
    if (strlen(s_app_key) > 8) {
        snprintf(masked, sizeof(masked), "%.*s...", 8, s_app_key);
    } else {
        strncpy(masked, s_app_key, sizeof(masked) - 1);
    }
    
    ESP_LOGI(TAG, "DingTalk bot initialized with app_key: %s", masked);
    
    return ESP_OK;
}

esp_err_t dingtalk_bot_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "DingTalk bot already running");
        return ESP_OK;
    }
    
    if (strlen(s_app_key) == 0 || strlen(s_app_secret) == 0) {
        ESP_LOGW(TAG, "DingTalk credentials not configured, skipping Stream mode");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Starting DingTalk Stream mode...");
    
    s_running = true;
    
    BaseType_t ret = xTaskCreatePinnedToCore(
        dingtalk_stream_task,
        "dt_stream",
        MIMI_DINGTALK_POLL_STACK,
        NULL,
        MIMI_DINGTALK_POLL_PRIO,
        &s_stream_task,
        MIMI_DINGTALK_POLL_CORE
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create Stream task");
        s_running = false;
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t dingtalk_bot_stop(void)
{
    if (!s_running) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Stopping DingTalk Stream mode...");
    
    s_running = false;
    
    if (s_stream_task) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_stream_task = NULL;
    }
    
    if (s_ws_transport) {
        esp_transport_close(s_ws_transport);
    }
    
    if (s_transport_list) {
        esp_transport_list_destroy(s_transport_list);
        s_transport_list = NULL;
        s_ws_transport = NULL;
    }
    
    ESP_LOGI(TAG, "DingTalk Stream mode stopped");
    return ESP_OK;
}

esp_err_t dingtalk_set_credentials(const char *app_key, const char *app_secret)
{
    if (!app_key || !app_secret) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(s_app_key, app_key, sizeof(s_app_key) - 1);
    strncpy(s_app_secret, app_secret, sizeof(s_app_secret) - 1);
    
    s_access_token[0] = '\0';
    s_token_expire_time = 0;
    
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(MIMI_NVS_DINGTALK, NVS_READWRITE, &nvs);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }
    
    nvs_set_str(nvs, MIMI_NVS_KEY_DINGTALK_KEY, s_app_key);
    nvs_set_str(nvs, MIMI_NVS_KEY_DINGTALK_SECRET, s_app_secret);
    nvs_commit(nvs);
    nvs_close(nvs);
    
    ESP_LOGI(TAG, "Credentials saved to NVS");
    
    return ESP_OK;
}

bool dingtalk_is_connected(void)
{
    return s_running && s_ws_transport != NULL;
}
