#include "ws_server.h"
#include "ottoclaw_config.h"
#include "bus/message_bus.h"

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"

static const char *TAG = "ws";

static httpd_handle_t s_server = NULL;

/* Simple client tracking */
typedef struct {
    int fd;
    char chat_id[32];
    bool active;
} ws_client_t;

static ws_client_t s_clients[OTTOCLAW_WS_MAX_CLIENTS];

static ws_client_t *find_client_by_fd(int fd)
{
    for (int i = 0; i < OTTOCLAW_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].fd == fd) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static ws_client_t *find_client_by_chat_id(const char *chat_id)
{
    for (int i = 0; i < OTTOCLAW_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && strcmp(s_clients[i].chat_id, chat_id) == 0) {
            return &s_clients[i];
        }
    }
    return NULL;
}

static ws_client_t *add_client(int fd)
{
    for (int i = 0; i < OTTOCLAW_WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
            s_clients[i].fd = fd;
            snprintf(s_clients[i].chat_id, sizeof(s_clients[i].chat_id), "ws_%d", fd);
            s_clients[i].active = true;
            ESP_LOGI(TAG, "Client connected: %s (fd=%d)", s_clients[i].chat_id, fd);
            return &s_clients[i];
        }
    }
    ESP_LOGW(TAG, "Max clients reached, rejecting fd=%d", fd);
    return NULL;
}

static void remove_client(int fd)
{
    for (int i = 0; i < OTTOCLAW_WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].fd == fd) {
            ESP_LOGI(TAG, "Client disconnected: %s", s_clients[i].chat_id);
            s_clients[i].active = false;
            return;
        }
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        /* WebSocket handshake — register client */
        int fd = httpd_req_to_sockfd(req);
        add_client(fd);
        return ESP_OK;
    }

    /* Receive WebSocket frame */
    httpd_ws_frame_t ws_pkt = {0};
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    /* Get frame length */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;

    if (ws_pkt.len == 0) return ESP_OK;

    ws_pkt.payload = calloc(1, ws_pkt.len + 1);
    if (!ws_pkt.payload) return ESP_ERR_NO_MEM;

    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        free(ws_pkt.payload);
        return ret;
    }

    int fd = httpd_req_to_sockfd(req);
    ws_client_t *client = find_client_by_fd(fd);

    /* Parse JSON message */
    cJSON *root = cJSON_Parse((char *)ws_pkt.payload);
    free(ws_pkt.payload);

    if (!root) {
        ESP_LOGW(TAG, "Invalid JSON from fd=%d", fd);
        return ESP_OK;
    }

    cJSON *type = cJSON_GetObjectItem(root, "type");
    cJSON *content = cJSON_GetObjectItem(root, "content");

    if (type && cJSON_IsString(type) && strcmp(type->valuestring, "message") == 0
        && content && cJSON_IsString(content)) {

        /* Determine chat_id */
        const char *chat_id = client ? client->chat_id : "ws_unknown";
        cJSON *cid = cJSON_GetObjectItem(root, "chat_id");
        if (cid && cJSON_IsString(cid)) {
            chat_id = cid->valuestring;
            /* Update client's chat_id if provided */
            if (client) {
                strncpy(client->chat_id, chat_id, sizeof(client->chat_id) - 1);
            }
        }

        ESP_LOGI(TAG, "WS message from %s: %.40s...", chat_id, content->valuestring);

        /* Push to inbound bus */
        ottoclaw_msg_t msg = {0};
        strncpy(msg.channel, OTTOCLAW_CHAN_WEBSOCKET, sizeof(msg.channel) - 1);
        strncpy(msg.chat_id, chat_id, sizeof(msg.chat_id) - 1);
        msg.content = strdup(content->valuestring);
        if (msg.content) {
            message_bus_push_inbound(&msg);
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

/* HTTP handler for serving the chat page */
static esp_err_t http_get_handler(httpd_req_t *req)
{
    const char *filepath = "/spiffs/www/index.html";
    
    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "text/html");
    
    char buf[512];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK) {
            fclose(f);
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    
    ESP_LOGI(TAG, "Served chat page to client");
    return ESP_OK;
}

/* HTTP handler for settings page */
static esp_err_t settings_get_handler(httpd_req_t *req)
{
    const char *filepath = "/spiffs/www/settings.html";
    
    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", filepath);
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "text/html");
    
    char buf[512];
    size_t read_bytes;
    while ((read_bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (httpd_resp_send_chunk(req, buf, read_bytes) != ESP_OK) {
            fclose(f);
            httpd_resp_send_chunk(req, NULL, 0);
            return ESP_FAIL;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    
    return ESP_OK;
}

/* API handler - Get current settings */
static esp_err_t api_settings_get_handler(httpd_req_t *req)
{
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open("ottoclaw_config", NVS_READONLY, &nvs_h);
    
    cJSON *root = cJSON_CreateObject();
    
    if (err == ESP_OK) {
        char buf[128];
        size_t len;
        
        len = sizeof(buf);
        if (nvs_get_str(nvs_h, "wifi_ssid", buf, &len) == ESP_OK) {
            cJSON_AddStringToObject(root, "wifi_ssid", buf);
        }
        
        len = sizeof(buf);
        if (nvs_get_str(nvs_h, "proxy_host", buf, &len) == ESP_OK) {
            cJSON_AddStringToObject(root, "proxy_host", buf);
        }
        
        len = sizeof(buf);
        if (nvs_get_str(nvs_h, "proxy_port", buf, &len) == ESP_OK) {
            cJSON_AddStringToObject(root, "proxy_port", buf);
        }
        
        nvs_close(nvs_h);
    }
    
    httpd_resp_set_type(req, "application/json");
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/* API handler - Save WiFi settings */
static esp_err_t api_wifi_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open("ottoclaw_config", NVS_READWRITE, &nvs_h);
    bool success = false;
    
    if (err == ESP_OK) {
        cJSON *ssid = cJSON_GetObjectItem(root, "wifi_ssid");
        cJSON *pass = cJSON_GetObjectItem(root, "wifi_pass");
        
        if (ssid && cJSON_IsString(ssid)) {
            nvs_set_str(nvs_h, "wifi_ssid", ssid->valuestring);
        }
        if (pass && cJSON_IsString(pass)) {
            nvs_set_str(nvs_h, "wifi_pass", pass->valuestring);
        }
        
        nvs_commit(nvs_h);
        nvs_close(nvs_h);
        success = true;
    }
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    const char *resp = success ? "{\"success\":true}" : "{\"success\":false}";
    httpd_resp_send(req, resp, strlen(resp));
    
    return ESP_OK;
}

/* API handler - Save proxy settings */
static esp_err_t api_proxy_post_handler(httpd_req_t *req)
{
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;
    
    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    nvs_handle_t nvs_h;
    esp_err_t err = nvs_open("ottoclaw_config", NVS_READWRITE, &nvs_h);
    bool success = false;
    
    if (err == ESP_OK) {
        cJSON *host = cJSON_GetObjectItem(root, "proxy_host");
        cJSON *port = cJSON_GetObjectItem(root, "proxy_port");
        
        if (host && cJSON_IsString(host)) {
            if (strlen(host->valuestring) > 0) {
                nvs_set_str(nvs_h, "proxy_host", host->valuestring);
            } else {
                nvs_erase_key(nvs_h, "proxy_host");
            }
        }
        if (port && cJSON_IsString(port)) {
            if (strlen(port->valuestring) > 0) {
                nvs_set_str(nvs_h, "proxy_port", port->valuestring);
            } else {
                nvs_erase_key(nvs_h, "proxy_port");
            }
        }
        
        nvs_commit(nvs_h);
        nvs_close(nvs_h);
        success = true;
    }
    
    cJSON_Delete(root);
    
    httpd_resp_set_type(req, "application/json");
    const char *resp = success ? "{\"success\":true}" : "{\"success\":false}";
    httpd_resp_send(req, resp, strlen(resp));
    
    return ESP_OK;
}

/* API handler - Reboot */
static esp_err_t api_reboot_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"success\":true}", 17);
    
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ESP_OK;
}

esp_err_t ws_server_start(void)
{
    memset(s_clients, 0, sizeof(s_clients));

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = OTTOCLAW_WS_PORT;
    config.ctrl_port = OTTOCLAW_WS_PORT + 1;
    config.max_open_sockets = OTTOCLAW_WS_MAX_CLIENTS;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket server: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Register HTTP GET for root (serves chat page) */
    httpd_uri_t http_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = http_get_handler,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &http_uri);

    /* Register settings page */
    httpd_uri_t settings_uri = {
        .uri = "/settings.html",
        .method = HTTP_GET,
        .handler = settings_get_handler,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &settings_uri);

    /* Register API endpoints */
    httpd_uri_t api_settings_get_uri = {
        .uri = "/api/settings",
        .method = HTTP_GET,
        .handler = api_settings_get_handler,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &api_settings_get_uri);

    httpd_uri_t api_wifi_post_uri = {
        .uri = "/api/settings/wifi",
        .method = HTTP_POST,
        .handler = api_wifi_post_handler,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &api_wifi_post_uri);

    httpd_uri_t api_proxy_post_uri = {
        .uri = "/api/settings/proxy",
        .method = HTTP_POST,
        .handler = api_proxy_post_handler,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &api_proxy_post_uri);

    httpd_uri_t api_reboot_uri = {
        .uri = "/api/system/reboot",
        .method = HTTP_POST,
        .handler = api_reboot_handler,
        .is_websocket = false,
    };
    httpd_register_uri_handler(s_server, &api_reboot_uri);

    /* Register WebSocket URI */
    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &ws_uri);

    ESP_LOGI(TAG, "WebSocket server started on port %d", OTTOCLAW_WS_PORT);
    return ESP_OK;
}

esp_err_t ws_server_send(const char *chat_id, const char *text)
{
    if (!s_server) return ESP_ERR_INVALID_STATE;

    ws_client_t *client = find_client_by_chat_id(chat_id);
    if (!client) {
        ESP_LOGW(TAG, "No WS client with chat_id=%s", chat_id);
        return ESP_ERR_NOT_FOUND;
    }

    /* Build response JSON */
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "response");
    cJSON_AddStringToObject(resp, "content", text);
    cJSON_AddStringToObject(resp, "chat_id", chat_id);

    char *json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    if (!json_str) return ESP_ERR_NO_MEM;

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json_str,
        .len = strlen(json_str),
    };

    esp_err_t ret = httpd_ws_send_frame_async(s_server, client->fd, &ws_pkt);
    free(json_str);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send to %s: %s", chat_id, esp_err_to_name(ret));
        remove_client(client->fd);
    }

    return ret;
}

esp_err_t ws_server_send_token(const char *chat_id, const char *token)
{
    if (!s_server) return ESP_ERR_INVALID_STATE;

    ws_client_t *client = find_client_by_chat_id(chat_id);
    if (!client) {
        ESP_LOGW(TAG, "No WS client with chat_id=%s", chat_id);
        return ESP_ERR_NOT_FOUND;
    }

    /* Build token JSON */
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "type", "token");
    cJSON_AddStringToObject(resp, "content", token);
    cJSON_AddStringToObject(resp, "chat_id", chat_id);

    char *json_str = cJSON_PrintUnformatted(resp);
    cJSON_Delete(resp);

    if (!json_str) return ESP_ERR_NO_MEM;

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)json_str,
        .len = strlen(json_str),
    };

    esp_err_t ret = httpd_ws_send_frame_async(s_server, client->fd, &ws_pkt);
    free(json_str);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send token to %s: %s", chat_id, esp_err_to_name(ret));
        remove_client(client->fd);
    }

    return ret;
}

esp_err_t ws_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "WebSocket server stopped");
    }
    return ESP_OK;
}
