/**
 * Web Search Tool - Using Aliyun Bailian App with built-in search capability
 * 
 * This implementation calls a Bailian app that has web search enabled,
 * instead of directly calling Brave Search API.
 */

#include "tool_web_search.h"
#include "mimi_config.h"
#include "proxy/http_proxy.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "nvs.h"
#include "cJSON.h"

static const char *TAG = "web_search";

/* Bailian App configuration */
#define BAILIAN_APP_ID      "758d9af49a7a460ba91ba16a7d7d767b"
#define BAILIAN_API_HOST    "dashscope.aliyuncs.com"
#define BAILIAN_API_PATH    "/api/v1/apps/" BAILIAN_APP_ID "/completion"

static char s_search_key[128] = {0};

#define SEARCH_BUF_SIZE     (32 * 1024)

/* ── Response accumulator ─────────────────────────────────────── */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} search_buf_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    search_buf_t *sb = (search_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        size_t needed = sb->len + evt->data_len;
        if (needed < sb->cap) {
            memcpy(sb->data + sb->len, evt->data, evt->data_len);
            sb->len += evt->data_len;
            sb->data[sb->len] = '\0';
        }
    }
    return ESP_OK;
}

/* ── Init ─────────────────────────────────────────────────────── */

esp_err_t tool_web_search_init(void)
{
    /* Use main API key for Bailian app */
    if (MIMI_SECRET_API_KEY[0] != '\0') {
        strncpy(s_search_key, MIMI_SECRET_API_KEY, sizeof(s_search_key) - 1);
    }
    
    /* Search-specific key overrides if set */
    if (MIMI_SECRET_SEARCH_KEY[0] != '\0') {
        strncpy(s_search_key, MIMI_SECRET_SEARCH_KEY, sizeof(s_search_key) - 1);
    }

    /* NVS overrides take highest priority (set via CLI) */
    nvs_handle_t nvs;
    if (nvs_open(MIMI_NVS_SEARCH, NVS_READONLY, &nvs) == ESP_OK) {
        char tmp[128] = {0};
        size_t len = sizeof(tmp);
        if (nvs_get_str(nvs, MIMI_NVS_KEY_API_KEY, tmp, &len) == ESP_OK && tmp[0]) {
            strncpy(s_search_key, tmp, sizeof(s_search_key) - 1);
        }
        nvs_close(nvs);
    }

    if (s_search_key[0]) {
        ESP_LOGI(TAG, "Web search initialized (using Bailian App: %s)", BAILIAN_APP_ID);
    } else {
        ESP_LOGW(TAG, "No API key for web search");
    }
    return ESP_OK;
}

/* ── Extract text from Bailian response ─────────────────────────── */

static void extract_bailian_response(const char *response_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(response_json);
    if (!root) {
        snprintf(output, output_size, "Error: Failed to parse Bailian response");
        return;
    }

    /* Bailian app response format:
     * {
     *   "output": {
     *     "text": "搜索结果内容..."
     *   },
     *   "usage": {...},
     *   "request_id": "..."
     * }
     */
    cJSON *output_obj = cJSON_GetObjectItem(root, "output");
    if (output_obj) {
        cJSON *text = cJSON_GetObjectItem(output_obj, "text");
        if (text && cJSON_IsString(text)) {
            snprintf(output, output_size, "%s", text->valuestring);
            cJSON_Delete(root);
            return;
        }
    }

    /* Check for error response */
    cJSON *code = cJSON_GetObjectItem(root, "code");
    cJSON *message = cJSON_GetObjectItem(root, "message");
    if (code && message) {
        snprintf(output, output_size, "Error: %s - %s", 
                 cJSON_IsString(code) ? code->valuestring : "unknown",
                 cJSON_IsString(message) ? message->valuestring : "unknown");
        cJSON_Delete(root);
        return;
    }

    snprintf(output, output_size, "Error: Unexpected response format");
    cJSON_Delete(root);
}

/* ── Direct HTTPS request to Bailian ─────────────────────────────── */

static esp_err_t search_bailian_direct(const char *post_data, search_buf_t *sb)
{
    char url[256];
    snprintf(url, sizeof(url), "https://%s%s", BAILIAN_API_HOST, BAILIAN_API_PATH);
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .user_data = sb,
        .timeout_ms = 30000,
        .buffer_size = 4096,
        .buffer_size_tx = 4096,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return ESP_FAIL;

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    
    /* Bailian uses Bearer token */
    char auth[192];
    snprintf(auth, sizeof(auth), "Bearer %s", s_search_key);
    esp_http_client_set_header(client, "Authorization", auth);
    
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) return err;
    if (status != 200) {
        ESP_LOGE(TAG, "Bailian API returned %d: %s", status, sb->data);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* ── Proxy HTTPS request to Bailian ──────────────────────────────── */

static esp_err_t search_bailian_via_proxy(const char *post_data, search_buf_t *sb)
{
    proxy_conn_t *conn = proxy_conn_open(BAILIAN_API_HOST, 443, 30000);
    if (!conn) return ESP_ERR_HTTP_CONNECT;

    int body_len = strlen(post_data);
    char header[512];
    int hlen = snprintf(header, sizeof(header),
        "POST %s HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/json\r\n"
        "Authorization: Bearer %s\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n",
        BAILIAN_API_PATH, BAILIAN_API_HOST, s_search_key, body_len);

    if (proxy_conn_write(conn, header, hlen) < 0) {
        proxy_conn_close(conn);
        return ESP_ERR_HTTP_WRITE_DATA;
    }

    if (proxy_conn_write(conn, post_data, body_len) < 0) {
        proxy_conn_close(conn);
        return ESP_ERR_HTTP_WRITE_DATA;
    }

    /* Read full response */
    char tmp[4096];
    size_t total = 0;
    while (1) {
        int n = proxy_conn_read(conn, tmp, sizeof(tmp), 30000);
        if (n <= 0) break;
        size_t copy = (total + n < sb->cap - 1) ? (size_t)n : sb->cap - 1 - total;
        if (copy > 0) {
            memcpy(sb->data + total, tmp, copy);
            total += copy;
        }
    }
    sb->data[total] = '\0';
    sb->len = total;
    proxy_conn_close(conn);

    /* Check status */
    int status = 0;
    if (total > 5 && strncmp(sb->data, "HTTP/", 5) == 0) {
        const char *sp = strchr(sb->data, ' ');
        if (sp) status = atoi(sp + 1);
    }

    /* Strip headers */
    char *body = strstr(sb->data, "\r\n\r\n");
    if (body) {
        body += 4;
        size_t blen = total - (body - sb->data);
        memmove(sb->data, body, blen);
        sb->len = blen;
        sb->data[sb->len] = '\0';
    }

    if (status != 200) {
        ESP_LOGE(TAG, "Bailian API returned %d via proxy: %s", status, sb->data);
        return ESP_FAIL;
    }
    return ESP_OK;
}

/* ── Execute ──────────────────────────────────────────────────── */

esp_err_t tool_web_search_execute(const char *input_json, char *output, size_t output_size)
{
    if (s_search_key[0] == '\0') {
        snprintf(output, output_size, "Error: No API key configured for web search");
        return ESP_ERR_INVALID_STATE;
    }

    /* Parse input to get query */
    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: Invalid input JSON");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *query = cJSON_GetObjectItem(input, "query");
    if (!query || !cJSON_IsString(query) || query->valuestring[0] == '\0') {
        cJSON_Delete(input);
        snprintf(output, output_size, "Error: Missing 'query' field");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Web search via Bailian App: %s", query->valuestring);

    /* Build Bailian app request body */
    cJSON *req = cJSON_CreateObject();
    cJSON *input_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(input_obj, "prompt", query->valuestring);
    cJSON_AddItemToObject(req, "input", input_obj);
    cJSON_AddItemToObject(req, "parameters", cJSON_CreateObject());
    cJSON_AddItemToObject(req, "debug", cJSON_CreateObject());
    
    char *post_data = cJSON_PrintUnformatted(req);
    cJSON_Delete(req);
    cJSON_Delete(input);

    if (!post_data) {
        snprintf(output, output_size, "Error: Failed to build request");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Request body: %s", post_data);

    /* Allocate response buffer from PSRAM */
    search_buf_t sb = {0};
    sb.data = heap_caps_calloc(1, SEARCH_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (!sb.data) {
        free(post_data);
        snprintf(output, output_size, "Error: Out of memory");
        return ESP_ERR_NO_MEM;
    }
    sb.cap = SEARCH_BUF_SIZE;

    /* Make HTTP request */
    esp_err_t err;
    if (http_proxy_is_enabled()) {
        err = search_bailian_via_proxy(post_data, &sb);
    } else {
        err = search_bailian_direct(post_data, &sb);
    }

    free(post_data);

    if (err != ESP_OK) {
        free(sb.data);
        snprintf(output, output_size, "Error: Bailian API request failed");
        return err;
    }

    ESP_LOGI(TAG, "Bailian response: %d bytes", (int)sb.len);

    /* Extract text from response */
    extract_bailian_response(sb.data, output, output_size);
    free(sb.data);

    ESP_LOGI(TAG, "Search complete, %d bytes result", (int)strlen(output));
    return ESP_OK;
}

esp_err_t tool_web_search_set_key(const char *api_key)
{
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open(MIMI_NVS_SEARCH, NVS_READWRITE, &nvs));
    ESP_ERROR_CHECK(nvs_set_str(nvs, MIMI_NVS_KEY_API_KEY, api_key));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);

    strncpy(s_search_key, api_key, sizeof(s_search_key) - 1);
    ESP_LOGI(TAG, "Search API key saved");
    return ESP_OK;
}
