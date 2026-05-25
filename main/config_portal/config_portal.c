/*
 * OttoClaw Configuration Portal
 *
 * Starts an AP + HTTP server for web-based device configuration.
 * Serves setup.html and provides JSON REST APIs for all settings.
 *
 * AP SSID: "OttoClaw-XXXX" (last 4 hex of MAC, no password)
 * HTTP port: 80
 * AP IP: 192.168.4.1
 */

#include "config_portal.h"
#include "ottoclaw_config.h"
#include "lcd/lcd_display.h"
#include "otto/otto_movements.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "cJSON.h"

static const char *TAG = "config_portal";

/* ------------------------------------------------------------------ */
/*  State                                                               */
/* ------------------------------------------------------------------ */
static httpd_handle_t s_server  = NULL;
static bool           s_running = false;
static esp_netif_t   *s_ap_netif = NULL;
static esp_netif_t   *s_sta_netif = NULL;
static char           s_ap_ssid[32] = "";

/* Signal from /api/setup/complete or /api/setup/reset */
#define PORTAL_DONE_BIT BIT0
static EventGroupHandle_t s_done_eg;

/* Reference to Otto robot (defined in ottoclaw.c) */
extern otto_t g_otto;

/* ------------------------------------------------------------------ */
/*  NVS helpers                                                         */
/* ------------------------------------------------------------------ */
static esp_err_t nvs_read_str(const char *ns, const char *key, char *buf, size_t buf_len)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_str(h, key, buf, &buf_len);
    nvs_close(h);
    return err;
}

static esp_err_t nvs_write_str(const char *ns, const char *key, const char *val)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, key, val);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* Safely read NVS string — returns empty string on failure */
static void nvs_safe_str(const char *ns, const char *key, char *buf, size_t len)
{
    buf[0] = '\0';
    nvs_read_str(ns, key, buf, len);
}

/* ------------------------------------------------------------------ */
/*  HTTP utility: send JSON response                                   */
/* ------------------------------------------------------------------ */
static esp_err_t send_json(httpd_req_t *req, int status, const char *json)
{
    char status_str[16];
    snprintf(status_str, sizeof(status_str), "%d OK", status);
    httpd_resp_set_status(req, status == 200 ? "200 OK" : status_str);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_sendstr(req, json);
}

/* Read full request body into a caller-provided buffer */
static int recv_body(httpd_req_t *req, char *buf, int buf_len)
{
    int total = req->content_len;
    if (total <= 0 || total >= buf_len) return -1;
    int received = 0;
    while (received < total) {
        int n = httpd_req_recv(req, buf + received, total - received);
        if (n <= 0) return -1;
        received += n;
    }
    buf[received] = '\0';
    return received;
}

/* ------------------------------------------------------------------ */
/*  GET /api/status — return all current config as JSON               */
/* ------------------------------------------------------------------ */
static esp_err_t handle_status(httpd_req_t *req)
{
    char ssid[64]={0}, provider[32]={0}, model[64]={0}, base_url[128]={0};
    char dt_key[64]={0}, dt_secret[64]={0};
    char proxy_host[64]={0}, proxy_port[8]={0};
    char search_key[64]={0};
    char bailian_app_id[64]={0};

    nvs_safe_str(OTTOCLAW_NVS_WIFI,     OTTOCLAW_NVS_KEY_SSID,            ssid,       sizeof(ssid));
    nvs_safe_str(OTTOCLAW_NVS_LLM,      OTTOCLAW_NVS_KEY_PROVIDER,        provider,   sizeof(provider));
    nvs_safe_str(OTTOCLAW_NVS_LLM,      OTTOCLAW_NVS_KEY_MODEL,           model,      sizeof(model));
    nvs_safe_str(OTTOCLAW_NVS_LLM,      OTTOCLAW_NVS_KEY_BASE_URL,        base_url,   sizeof(base_url));
    nvs_safe_str(OTTOCLAW_NVS_DINGTALK, OTTOCLAW_NVS_KEY_DINGTALK_KEY,    dt_key,     sizeof(dt_key));
    nvs_safe_str(OTTOCLAW_NVS_DINGTALK, OTTOCLAW_NVS_KEY_DINGTALK_SECRET, dt_secret,  sizeof(dt_secret));
    nvs_safe_str(OTTOCLAW_NVS_PROXY,    OTTOCLAW_NVS_KEY_PROXY_HOST,      proxy_host, sizeof(proxy_host));
    nvs_safe_str(OTTOCLAW_NVS_PROXY,    OTTOCLAW_NVS_KEY_PROXY_PORT,      proxy_port, sizeof(proxy_port));
    nvs_safe_str(OTTOCLAW_NVS_SEARCH,   OTTOCLAW_NVS_KEY_SEARCH_KEY,      search_key, sizeof(search_key));
    nvs_safe_str(OTTOCLAW_NVS_SEARCH,   OTTOCLAW_NVS_KEY_BAILIAN_APP_ID,  bailian_app_id, sizeof(bailian_app_id));

    const char *provider_value = provider[0] ? provider : OTTOCLAW_LLM_PROVIDER_DEFAULT;
    const char *model_value    = model[0]    ? model    : OTTOCLAW_LLM_DEFAULT_MODEL;
    bool wifi_configured       = ssid[0] != '\0';
    bool llm_configured        = provider_value[0] != '\0';
    bool dingtalk_configured   = dt_key[0] != '\0' && dt_secret[0] != '\0';
    bool complete              = wifi_configured && llm_configured;

    cJSON *root = cJSON_CreateObject();
    cJSON *wifi = cJSON_AddObjectToObject(root, "wifi");
    cJSON *llm = cJSON_AddObjectToObject(root, "llm");
    cJSON *dingtalk = cJSON_AddObjectToObject(root, "dingtalk");
    cJSON *proxy = cJSON_AddObjectToObject(root, "proxy");
    cJSON *search = cJSON_AddObjectToObject(root, "search");

    cJSON_AddBoolToObject(root, "ok", true);
    cJSON_AddBoolToObject(root, "complete", complete);
    cJSON_AddStringToObject(root, "ap_ssid", s_ap_ssid[0] ? s_ap_ssid : "OttoClaw");

    cJSON_AddBoolToObject(wifi, "configured", wifi_configured);
    cJSON_AddStringToObject(wifi, "ssid", ssid);

    cJSON_AddBoolToObject(llm, "configured", llm_configured);
    cJSON_AddStringToObject(llm, "provider", provider_value);
    cJSON_AddStringToObject(llm, "model", model_value);
    cJSON_AddStringToObject(llm, "base_url", base_url);

    cJSON_AddBoolToObject(dingtalk, "configured", dingtalk_configured);
    cJSON_AddStringToObject(dingtalk, "app_key", dt_key);
    cJSON_AddStringToObject(dingtalk, "app_secret", dt_secret);

    cJSON_AddStringToObject(proxy, "host", proxy_host);
    cJSON_AddStringToObject(proxy, "port", proxy_port);

    cJSON_AddBoolToObject(search, "configured", search_key[0] != '\0');
    cJSON_AddStringToObject(search, "key", search_key);
    cJSON_AddStringToObject(search, "bailian_app_id", bailian_app_id);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    esp_err_t ret = send_json(req, 200, json ? json : "{}");
    free(json);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  GET /api/wifi/scan — scan nearby APs                              */
/* ------------------------------------------------------------------ */
static esp_err_t handle_wifi_scan(httpd_req_t *req)
{
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = false,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_cfg, true /* blocking */);
    if (err == ESP_ERR_WIFI_STATE) {
        ESP_LOGW(TAG, "WiFi scan busy in current state, restarting WiFi and retrying");
        esp_wifi_stop();
        vTaskDelay(pdMS_TO_TICKS(200));
        esp_wifi_start();
        vTaskDelay(pdMS_TO_TICKS(200));
        err = esp_wifi_scan_start(&scan_cfg, true /* blocking */);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi scan failed: %s", esp_err_to_name(err));
        return send_json(req, 200, "{\"ok\":false,\"aps\":[],\"msg\":\"wifi scan failed\"}");
    }

    uint16_t count = 0;
    esp_wifi_scan_get_ap_num(&count);
    if (count > 20) count = 20;

    cJSON *root = cJSON_CreateObject();
    cJSON *arr = cJSON_AddArrayToObject(root, "aps");
    cJSON_AddBoolToObject(root, "ok", true);

    if (count == 0) {
        char *json = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        esp_err_t ret = send_json(req, 200, json ? json : "{\"ok\":true,\"aps\":[]}");
        free(json);
        return ret;
    }

    wifi_ap_record_t *list = calloc(count, sizeof(wifi_ap_record_t));
    if (!list) {
        cJSON_Delete(root);
        return send_json(req, 200, "{\"ok\":false,\"aps\":[],\"msg\":\"out of memory\"}");
    }

    uint16_t max = count;
    err = esp_wifi_scan_get_ap_records(&max, list);
    if (err != ESP_OK) {
        free(list);
        cJSON_Delete(root);
        ESP_LOGE(TAG, "WiFi scan read failed: %s", esp_err_to_name(err));
        return send_json(req, 200, "{\"ok\":false,\"aps\":[],\"msg\":\"wifi scan read failed\"}");
    }

    for (uint16_t i = 0; i < max; i++) {
        if (list[i].ssid[0] == '\0') continue;
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (char *)list[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", list[i].rssi);
        cJSON_AddNumberToObject(ap, "auth", list[i].authmode);
        cJSON_AddItemToArray(arr, ap);
    }
    free(list);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    esp_err_t ret = send_json(req, 200, json ? json : "{\"ok\":true,\"aps\":[]}");
    free(json);
    return ret;
}

/* ------------------------------------------------------------------ */
/*  POST /api/wifi/connect — save WiFi credentials                    */
/* ------------------------------------------------------------------ */
static char s_req_body[512];

static esp_err_t handle_wifi_connect(httpd_req_t *req)
{
    if (recv_body(req, s_req_body, sizeof(s_req_body)) < 0)
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"body too large\"}");

    cJSON *root = cJSON_Parse(s_req_body);
    if (!root) return send_json(req, 200, "{\"ok\":false,\"msg\":\"invalid JSON\"}");

    const char *ssid = cJSON_GetStringValue(cJSON_GetObjectItem(root, "ssid"));
    const char *pass = cJSON_GetStringValue(cJSON_GetObjectItem(root, "password"));
    if (!ssid || ssid[0] == '\0') {
        cJSON_Delete(root);
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"ssid required\"}");
    }
    if (!pass) pass = "";

    nvs_write_str(OTTOCLAW_NVS_WIFI, OTTOCLAW_NVS_KEY_SSID, ssid);
    nvs_write_str(OTTOCLAW_NVS_WIFI, OTTOCLAW_NVS_KEY_PASS,  pass);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "WiFi credentials saved: %s", ssid);
    return send_json(req, 200, "{\"ok\":true,\"msg\":\"WiFi credentials saved\"}");
}

/* ------------------------------------------------------------------ */
/*  POST /api/llm/config                                              */
/* ------------------------------------------------------------------ */
static esp_err_t handle_llm_config(httpd_req_t *req)
{
    if (recv_body(req, s_req_body, sizeof(s_req_body)) < 0)
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"body too large\"}");

    cJSON *root = cJSON_Parse(s_req_body);
    if (!root) return send_json(req, 200, "{\"ok\":false,\"msg\":\"invalid JSON\"}");

    const char *api_key  = cJSON_GetStringValue(cJSON_GetObjectItem(root, "api_key"));
    const char *model    = cJSON_GetStringValue(cJSON_GetObjectItem(root, "model"));
    const char *provider = cJSON_GetStringValue(cJSON_GetObjectItem(root, "provider"));
    const char *base_url = cJSON_GetStringValue(cJSON_GetObjectItem(root, "base_url"));

    if (api_key  && api_key[0])  nvs_write_str(OTTOCLAW_NVS_LLM, OTTOCLAW_NVS_KEY_API_KEY,  api_key);
    if (model    && model[0])    nvs_write_str(OTTOCLAW_NVS_LLM, OTTOCLAW_NVS_KEY_MODEL,    model);
    if (provider && provider[0]) nvs_write_str(OTTOCLAW_NVS_LLM, OTTOCLAW_NVS_KEY_PROVIDER, provider);
    if (base_url)                nvs_write_str(OTTOCLAW_NVS_LLM, OTTOCLAW_NVS_KEY_BASE_URL, base_url);

    cJSON_Delete(root);
    ESP_LOGI(TAG, "LLM config saved");
    return send_json(req, 200, "{\"ok\":true,\"msg\":\"LLM config saved\"}");
}

/* ------------------------------------------------------------------ */
/*  POST /api/dingtalk/config                                         */
/* ------------------------------------------------------------------ */
static esp_err_t handle_dingtalk_config(httpd_req_t *req)
{
    if (recv_body(req, s_req_body, sizeof(s_req_body)) < 0)
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"body too large\"}");

    cJSON *root = cJSON_Parse(s_req_body);
    if (!root) return send_json(req, 200, "{\"ok\":false,\"msg\":\"invalid JSON\"}");

    const char *app_key    = cJSON_GetStringValue(cJSON_GetObjectItem(root, "app_key"));
    const char *app_secret = cJSON_GetStringValue(cJSON_GetObjectItem(root, "app_secret"));

    if (app_key    && app_key[0])    nvs_write_str(OTTOCLAW_NVS_DINGTALK, OTTOCLAW_NVS_KEY_DINGTALK_KEY,    app_key);
    if (app_secret && app_secret[0]) nvs_write_str(OTTOCLAW_NVS_DINGTALK, OTTOCLAW_NVS_KEY_DINGTALK_SECRET, app_secret);

    cJSON_Delete(root);
    ESP_LOGI(TAG, "DingTalk config saved");
    return send_json(req, 200, "{\"ok\":true,\"msg\":\"DingTalk config saved\"}");
}

/* ------------------------------------------------------------------ */
/*  POST /api/proxy/config                                            */
/* ------------------------------------------------------------------ */
static esp_err_t handle_proxy_config(httpd_req_t *req)
{
    if (recv_body(req, s_req_body, sizeof(s_req_body)) < 0)
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"body too large\"}");

    cJSON *root = cJSON_Parse(s_req_body);
    if (!root) return send_json(req, 200, "{\"ok\":false,\"msg\":\"invalid JSON\"}");

    const char *host = cJSON_GetStringValue(cJSON_GetObjectItem(root, "host"));
    const char *port = cJSON_GetStringValue(cJSON_GetObjectItem(root, "port"));

    if (host) nvs_write_str(OTTOCLAW_NVS_PROXY, OTTOCLAW_NVS_KEY_PROXY_HOST, host);
    if (port) nvs_write_str(OTTOCLAW_NVS_PROXY, OTTOCLAW_NVS_KEY_PROXY_PORT, port);

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Proxy config saved");
    return send_json(req, 200, "{\"ok\":true,\"msg\":\"Proxy config saved\"}");
}

/* ------------------------------------------------------------------ */
/*  POST /api/search/config                                           */
/* ------------------------------------------------------------------ */
static esp_err_t handle_search_config(httpd_req_t *req)
{
    if (recv_body(req, s_req_body, sizeof(s_req_body)) < 0)
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"body too large\"}");

    cJSON *root = cJSON_Parse(s_req_body);
    if (!root) return send_json(req, 200, "{\"ok\":false,\"msg\":\"invalid JSON\"}");

    const char *key = cJSON_GetStringValue(cJSON_GetObjectItem(root, "search_key"));
    const char *app_id = cJSON_GetStringValue(cJSON_GetObjectItem(root, "bailian_app_id"));
    if (key) nvs_write_str(OTTOCLAW_NVS_SEARCH, OTTOCLAW_NVS_KEY_SEARCH_KEY, key);
    if (app_id && app_id[0]) nvs_write_str(OTTOCLAW_NVS_SEARCH, OTTOCLAW_NVS_KEY_BAILIAN_APP_ID, app_id);

    cJSON_Delete(root);
    return send_json(req, 200, "{\"ok\":true,\"msg\":\"Search config saved\"}");
}

/* ------------------------------------------------------------------ */
/*  POST /api/otto/action — test Otto movements                       */
/* ------------------------------------------------------------------ */
static esp_err_t handle_otto_action(httpd_req_t *req)
{
    if (recv_body(req, s_req_body, sizeof(s_req_body)) < 0)
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"body too large\"}");

    cJSON *root = cJSON_Parse(s_req_body);
    if (!root) return send_json(req, 200, "{\"ok\":false,\"msg\":\"invalid JSON\"}");

    const char *action = cJSON_GetStringValue(cJSON_GetObjectItem(root, "action"));
    if (!action) {
        cJSON_Delete(root);
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"action required\"}");
    }

    ESP_LOGI(TAG, "Otto action: %s", action);

    /* Execute action in current task — keep it short */
    if      (strcmp(action, "home")          == 0) otto_home(&g_otto, true);
    else if (strcmp(action, "walk_fwd")      == 0) otto_walk(&g_otto, 3, 1000, FORWARD, MEDIUM);
    else if (strcmp(action, "walk_bwd")      == 0) otto_walk(&g_otto, 3, 1000, BACKWARD, MEDIUM);
    else if (strcmp(action, "turn_left")     == 0) otto_turn(&g_otto, 3, 1000, LEFT, MEDIUM);
    else if (strcmp(action, "turn_right")    == 0) otto_turn(&g_otto, 3, 1000, RIGHT, MEDIUM);
    else if (strcmp(action, "bend_left")     == 0) otto_bend(&g_otto, 1, 800, LEFT);
    else if (strcmp(action, "bend_right")    == 0) otto_bend(&g_otto, 1, 800, RIGHT);
    else if (strcmp(action, "shake_left")    == 0) otto_shake_leg(&g_otto, 1, 1500, LEFT);
    else if (strcmp(action, "shake_right")   == 0) otto_shake_leg(&g_otto, 1, 1500, RIGHT);
    else if (strcmp(action, "jump")          == 0) otto_jump(&g_otto, 1, 2000);
    else if (strcmp(action, "updown")        == 0) otto_updown(&g_otto, 3, 800, MEDIUM);
    else if (strcmp(action, "swing")         == 0) otto_swing(&g_otto, 3, 800, MEDIUM);
    else if (strcmp(action, "tiptoe")        == 0) otto_tiptoe_swing(&g_otto, 3, 900, MEDIUM);
    else if (strcmp(action, "jitter")        == 0) otto_jitter(&g_otto, 3, 500, SMALL);
    else if (strcmp(action, "asc_turn")      == 0) otto_ascending_turn(&g_otto, 3, 900, MEDIUM);
    else if (strcmp(action, "moonwalker_l")  == 0) otto_moonwalker(&g_otto, 3, 900, MEDIUM, LEFT);
    else if (strcmp(action, "moonwalker_r")  == 0) otto_moonwalker(&g_otto, 3, 900, MEDIUM, RIGHT);
    else if (strcmp(action, "crusaito_l")    == 0) otto_crusaito(&g_otto, 3, 900, MEDIUM, LEFT);
    else if (strcmp(action, "crusaito_r")    == 0) otto_crusaito(&g_otto, 3, 900, MEDIUM, RIGHT);
    else if (strcmp(action, "flapping_l")    == 0) otto_flapping(&g_otto, 3, 1000, MEDIUM, LEFT);
    else if (strcmp(action, "flapping_r")    == 0) otto_flapping(&g_otto, 3, 1000, MEDIUM, RIGHT);
    else if (strcmp(action, "hands_up_l")    == 0) otto_hands_up(&g_otto, 1000, LEFT);
    else if (strcmp(action, "hands_up_r")    == 0) otto_hands_up(&g_otto, 1000, RIGHT);
    else if (strcmp(action, "hands_down_l")  == 0) otto_hands_down(&g_otto, 1000, LEFT);
    else if (strcmp(action, "hands_down_r")  == 0) otto_hands_down(&g_otto, 1000, RIGHT);
    else if (strcmp(action, "hand_wave_l")   == 0) otto_hand_wave(&g_otto, LEFT);
    else if (strcmp(action, "hand_wave_r")   == 0) otto_hand_wave(&g_otto, RIGHT);
    else if (strcmp(action, "windmill")      == 0) otto_windmill(&g_otto, 3, 1000, MEDIUM);
    else if (strcmp(action, "takeoff")       == 0) otto_takeoff(&g_otto, 3, 1000, MEDIUM);
    else if (strcmp(action, "fitness")       == 0) otto_fitness(&g_otto, 3, 1000, MEDIUM);
    else if (strcmp(action, "greeting_l")    == 0) otto_greeting(&g_otto, LEFT, 1);
    else if (strcmp(action, "greeting_r")    == 0) otto_greeting(&g_otto, RIGHT, 1);
    else if (strcmp(action, "shy_l")         == 0) otto_shy(&g_otto, LEFT, 1);
    else if (strcmp(action, "shy_r")         == 0) otto_shy(&g_otto, RIGHT, 1);
    else if (strcmp(action, "sit")           == 0) otto_sit(&g_otto);
    else if (strcmp(action, "whirlwind")     == 0) otto_whirlwind_leg(&g_otto, 3, 800, MEDIUM);
    else if (strcmp(action, "radio")         == 0) otto_radio_calisthenics(&g_otto);
    else if (strcmp(action, "magic_circle")  == 0) otto_magic_circle(&g_otto);
    else if (strcmp(action, "showcase")      == 0) otto_showcase(&g_otto);
    else {
        cJSON_Delete(root);
        return send_json(req, 200, "{\"ok\":false,\"msg\":\"unknown action\"}");
    }

    if (strcmp(action, "home") != 0) {
        otto_home(&g_otto, true);
    }

    cJSON_Delete(root);
    return send_json(req, 200, "{\"ok\":true}");
}

/* ------------------------------------------------------------------ */
/*  POST /api/setup/complete — save all and reboot                    */
/* ------------------------------------------------------------------ */
static esp_err_t handle_setup_complete(httpd_req_t *req)
{
    send_json(req, 200, "{\"ok\":true,\"msg\":\"Saved. Rebooting in 2 seconds...\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    xEventGroupSetBits(s_done_eg, PORTAL_DONE_BIT);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  POST /api/setup/reset — erase NVS and reboot                     */
/* ------------------------------------------------------------------ */
static esp_err_t handle_setup_reset(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Factory reset requested via portal");
    send_json(req, 200, "{\"ok\":true,\"msg\":\"Factory reset. Rebooting...\"}");
    vTaskDelay(pdMS_TO_TICKS(500));
    nvs_flash_erase();
    xEventGroupSetBits(s_done_eg, PORTAL_DONE_BIT);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  GET / → redirect to /setup.html                                   */
/* ------------------------------------------------------------------ */
static esp_err_t handle_root(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/setup.html");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  GET /setup.html — serve embedded HTML                             */
/* ------------------------------------------------------------------ */
/* The HTML is embedded as a C string. It's ~8KB — well within HTTP  */
/* send buffer and SPIFFS-less operation.                              */

static const char SETUP_HTML[] =
"<!DOCTYPE html><html lang='zh'><head><meta charset='UTF-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>OttoClaw 配置</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;"
"background:#0a0a1a;color:#e0e0ff;min-height:100vh;padding:16px}"
"h1{text-align:center;color:#7eb8ff;font-size:1.5em;margin-bottom:16px}"
"h1 span{font-size:.6em;display:block;color:#888;margin-top:4px}"
".tabs{display:flex;gap:4px;flex-wrap:wrap;margin-bottom:16px}"
".tab{padding:8px 14px;border:1px solid #333;border-radius:8px;cursor:pointer;"
"background:#111;color:#aaa;font-size:.85em;transition:.2s}"
".tab.active{background:#1a3a6a;border-color:#7eb8ff;color:#7eb8ff}"
".panel{display:none}.panel.active{display:block}"
"label{display:block;font-size:.8em;color:#888;margin-bottom:4px;margin-top:12px}"
"input,select{width:100%;padding:10px;border:1px solid #333;border-radius:8px;"
"background:#111;color:#e0e0ff;font-size:.9em;outline:none}"
"input:focus,select:focus{border-color:#7eb8ff}"
".btn{display:inline-block;padding:10px 20px;border:none;border-radius:8px;"
"cursor:pointer;font-size:.9em;font-weight:600;margin:4px 2px;transition:.2s}"
".btn-primary{background:#1a4a8a;color:#7eb8ff;border:1px solid #7eb8ff}"
".btn-primary:hover{background:#2a5a9a}"
".btn-danger{background:#4a1a1a;color:#ff7e7e;border:1px solid #ff5555}"
".btn-danger:hover{background:#5a2a2a}"
".btn-success{background:#1a4a1a;color:#7eff7e;border:1px solid #55ff55}"
".btn-success:hover{background:#2a5a2a}"
".btn-sm{padding:6px 12px;font-size:.8em}"
".actions{margin-top:20px;display:flex;gap:8px;flex-wrap:wrap}"
".status{margin-top:8px;padding:8px;border-radius:6px;font-size:.8em;min-height:32px}"
".status.ok{background:#0a3a0a;color:#7eff7e;border:1px solid #2a5a2a}"
".status.err{background:#3a0a0a;color:#ff7e7e;border:1px solid #5a2a2a}"
".status.info{background:#0a1a3a;color:#7eb8ff;border:1px solid #1a2a5a}"
".ap-list{margin-top:8px}"
".ap-item{padding:8px;margin:4px 0;border:1px solid #222;border-radius:6px;"
"cursor:pointer;background:#111;display:flex;justify-content:space-between}"
".ap-item:hover{border-color:#7eb8ff;background:#0a1a3a}"
".ap-rssi{color:#888;font-size:.8em}"
".otto-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:6px;margin-top:8px}"
".otto-btn{padding:8px 4px;border:1px solid #333;border-radius:6px;background:#111;"
"color:#aaa;cursor:pointer;font-size:.75em;text-align:center;transition:.2s}"
".otto-btn:hover{border-color:#7eff7e;color:#7eff7e;background:#0a1a0a}"
".section-title{color:#7eb8ff;font-size:.85em;font-weight:600;margin-top:16px;margin-bottom:4px;border-bottom:1px solid #222;padding-bottom:4px}"
"</style></head><body>"
"<h1>🤖 OttoClaw<span>初始配置 · 闪猫科技研发 · 配置完成后点「保存并重启」</span></h1>"
"<div class='tabs'>"
"<div class='tab active' onclick='showTab(\"wifi\")'>📡 WiFi</div>"
"<div class='tab' onclick='showTab(\"llm\")'>🤖 大模型</div>"
"<div class='tab' onclick='showTab(\"dt\")'>🔔 钉钉</div>"
"<div class='tab' onclick='showTab(\"other\")'>⚙️ 其他</div>"
"<div class='tab' onclick='showTab(\"otto\")'>🦾 动作测试</div>"
"</div>"

/* ── WiFi ── */
"<div id='panel-wifi' class='panel active'>"
"<div class='section-title'>扫描 WiFi</div>"
"<button class='btn btn-primary btn-sm' onclick='scanWifi()'>🔍 扫描周边</button>"
"<div id='ap-list' class='ap-list'></div>"
"<label>SSID</label><input id='wifi-ssid' placeholder='WiFi名称'>"
"<label>密码</label><input id='wifi-pass' type='password' placeholder='WiFi密码'>"
"<button class='btn btn-primary' style='margin-top:10px' onclick='saveWifi()'>💾 保存WiFi</button>"
"<div id='wifi-status' class='status info'></div>"
"</div>"

/* ── LLM ── */
"<div id='panel-llm' class='panel'>"
"<label>服务商</label>"
"<select id='llm-provider'>"
"<option value='anthropic'>Anthropic (Claude)</option>"
"<option value='openai'>OpenAI</option>"
"<option value='qwen'>通义千问 (Qwen)</option>"
"<option value='deepseek'>DeepSeek</option>"
"<option value='zhipu'>智谱 (ZhiPu)</option>"
"<option value='moonshot'>月之暗面 (Moonshot)</option>"
"<option value='groq'>Groq</option>"
"<option value='vllm'>自定义 (vLLM/Other)</option>"
"</select>"
"<label>API Key</label><input id='llm-key' type='password' placeholder='sk-...'>"
"<label>模型名称</label><input id='llm-model' placeholder='claude-opus-4-5'>"
"<label>Base URL (可选，留空用默认)</label>"
"<input id='llm-url' placeholder='https://api.anthropic.com/v1/messages'>"
"<button class='btn btn-primary' style='margin-top:10px' onclick='saveLlm()'>💾 保存</button>"
"<div id='llm-status' class='status info'></div>"
"</div>"

/* ── DingTalk ── */
"<div id='panel-dt' class='panel'>"
"<label>App Key</label><input id='dt-key' placeholder='dingxxxxxxxx'>"
"<label>App Secret</label><input id='dt-secret' type='password' placeholder='secret...'>"
"<button class='btn btn-primary' style='margin-top:10px' onclick='saveDt()'>💾 保存</button>"
"<div id='dt-status' class='status info'></div>"
"</div>"

/* ── Other ── */
"<div id='panel-other' class='panel'>"
"<div class='section-title'>HTTP 代理</div>"
"<label>代理 Host</label><input id='proxy-host' placeholder='192.168.1.100'>"
"<label>代理 Port</label><input id='proxy-port' placeholder='7890'>"
"<button class='btn btn-primary' style='margin-top:10px' onclick='saveProxy()'>💾 保存代理</button>"
"<div class='section-title' style='margin-top:16px'>搜索 (DashScope)</div>"
"<label>搜索 API Key</label><input id='search-key' placeholder='sk-...'>"
"<label>百炼搜索 App ID</label><input id='bailian-app-id' placeholder='758d9af4...'>"
"<p style='color:#888;font-size:.75em;margin-top:2px'>阿里云百炼平台创建的搜索应用 ID，用于联网搜索功能</p>"
"<button class='btn btn-primary' style='margin-top:10px' onclick='saveSearch()'>💾 保存搜索配置</button>"
"<div id='other-status' class='status info'></div>"
"</div>"

/* ── Otto ── */
"<div id='panel-otto' class='panel'>"
"<p style='color:#888;font-size:.8em;margin-bottom:8px'>点击按钮测试对应动作</p>"
"<div class='otto-grid' id='otto-grid'></div>"
"<div id='otto-status' class='status info'></div>"
"</div>"

/* ── Bottom buttons ── */
"<div class='actions'>"
"<button class='btn btn-success' onclick='saveAll()'>✅ 保存并重启</button>"
"<button class='btn btn-danger' onclick='factoryReset()'>⚠️ 初始化设备</button>"
"</div>"
"<div id='main-status' class='status info' style='margin-top:8px'></div>"

"<script>"
"const ACTIONS=["
"['home','归位'],['walk_fwd','前进'],['walk_bwd','后退'],['turn_left','左转'],['turn_right','右转'],"
"['bend_left','左弯'],['bend_right','右弯'],['shake_left','抖左腿'],['shake_right','抖右腿'],"
"['jump','跳跃'],['updown','上下'],['swing','摇摆'],['tiptoe','踮脚'],['jitter','抖动'],"
"['asc_turn','升腾转'],['moonwalker_l','太空步L'],['moonwalker_r','太空步R'],"
"['crusaito_l','克鲁赛托L'],['crusaito_r','克鲁赛托R'],"
"['flapping_l','扑翼L'],['flapping_r','扑翼R'],"
"['hands_up_l','举手L'],['hands_up_r','举手R'],['hands_down_l','放手L'],['hands_down_r','放手R'],"
"['hand_wave_l','挥手L'],['hand_wave_r','挥手R'],['windmill','风车'],['takeoff','起飞'],"
"['fitness','健身'],['greeting_l','打招呼L'],['greeting_r','打招呼R'],"
"['shy_l','害羞L'],['shy_r','害羞R'],['sit','蹲下'],['whirlwind','旋风'],"
"['radio','广播体操'],['magic_circle','魔法圈'],['showcase','精彩展示']];"

"const grid=document.getElementById('otto-grid');"
"ACTIONS.forEach(([a,n])=>{"
"const b=document.createElement('button');"
"b.className='otto-btn';b.textContent=n;"
"b.onclick=()=>ottoAction(a,b);"
"grid.appendChild(b);"
"});"

"function showTab(t){"
"document.querySelectorAll('.tab').forEach((el,i)=>{"
"const tabs=['wifi','llm','dt','other','otto'];"
"el.classList.toggle('active',tabs[i]===t)"
"});"
"document.querySelectorAll('.panel').forEach(el=>el.classList.remove('active'));"
"document.getElementById('panel-'+t).classList.add('active');"
"}"

"async function api(path,body){"
"try{"
"const r=await fetch(path,body?{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)}:{});"
"return await r.json();"
"}catch(e){return{ok:false,msg:e.message};}"
"}"

"function setStatus(id,ok,msg){"
"const el=document.getElementById(id);"
"el.className='status '+(ok?'ok':'err');"
"el.textContent=msg;"
"}"

"async function loadStatus(){"
"const d=await api('/api/status');"
"if(!d)return;"
"const wifi=d.wifi||{};"
"const llm=d.llm||{};"
"const dingtalk=d.dingtalk||{};"
"const proxy=d.proxy||{};"
"const search=d.search||{};"
"document.getElementById('wifi-ssid').value=wifi.ssid||'';"
"document.getElementById('llm-key').value='';"
"document.getElementById('llm-model').value=llm.model||'';"
"document.getElementById('llm-url').value=llm.base_url||'';"
"const sel=document.getElementById('llm-provider');"
"if(llm.provider){"
"for(let o of sel.options)if(o.value===llm.provider){o.selected=true;break;}"
"}"
"document.getElementById('dt-key').value=dingtalk.app_key||'';"
"document.getElementById('dt-secret').value='';"
"document.getElementById('proxy-host').value=proxy.host||'';"
"document.getElementById('proxy-port').value=proxy.port||'';"
"document.getElementById('search-key').value=search.key||'';"
"document.getElementById('bailian-app-id').value=search.bailian_app_id||'';"
"}"

"async function scanWifi(){"
"document.getElementById('ap-list').innerHTML='<div style=\"color:#888;font-size:.8em;padding:8px\">扫描中...</div>';"
"const d=await api('/api/wifi/scan');"
"const list=document.getElementById('ap-list');"
"list.innerHTML='';"
"const aps=Array.isArray(d&&d.aps)?d.aps:[];"
"if(!d||!d.ok){list.innerHTML='<div style=\"color:#888;font-size:.8em;padding:8px\">扫描失败</div>';setStatus('wifi-status',false,d&&d.msg?d.msg:'扫描失败');return;}"
"if(!aps.length){list.innerHTML='<div style=\"color:#888;font-size:.8em;padding:8px\">未找到WiFi</div>';return;}"
"aps.sort((a,b)=>b.rssi-a.rssi).forEach(ap=>{"
"const div=document.createElement('div');"
"div.className='ap-item';"
"div.innerHTML='<span>'+ap.ssid+'</span><span class=\"ap-rssi\">'+ap.rssi+'dBm'+(ap.auth>0?' 🔒':'')+'</span>';"
"div.onclick=()=>{document.getElementById('wifi-ssid').value=ap.ssid;document.getElementById('wifi-pass').focus();};"
"list.appendChild(div);"
"});"
"setStatus('wifi-status',true,'已扫描到 '+aps.length+' 个网络');"
"}"

"async function saveWifi(){"
"const r=await api('/api/wifi/connect',{ssid:document.getElementById('wifi-ssid').value,password:document.getElementById('wifi-pass').value});"
"setStatus('wifi-status',r&&r.ok,r?(r.msg||'已保存'):'失败');"
"}"

"async function saveLlm(){"
"const r=await api('/api/llm/config',{"
"api_key:document.getElementById('llm-key').value,"
"model:document.getElementById('llm-model').value,"
"provider:document.getElementById('llm-provider').value,"
"base_url:document.getElementById('llm-url').value"
"});"
"setStatus('llm-status',r&&r.ok,r?(r.msg||'已保存'):'失败');"
"}"

"async function saveDt(){"
"const r=await api('/api/dingtalk/config',{app_key:document.getElementById('dt-key').value,app_secret:document.getElementById('dt-secret').value});"
"setStatus('dt-status',r&&r.ok,r?(r.msg||'已保存'):'失败');"
"}"

"async function saveProxy(){"
"const r=await api('/api/proxy/config',{host:document.getElementById('proxy-host').value,port:document.getElementById('proxy-port').value});"
"setStatus('other-status',r&&r.ok,r?(r.msg||'代理已保存'):'失败');"
"}"

"async function saveSearch(){"
"const r=await api('/api/search/config',{search_key:document.getElementById('search-key').value,bailian_app_id:document.getElementById('bailian-app-id').value});"
"setStatus('other-status',r&&r.ok,r?(r.msg||'搜索配置已保存'):'失败');"
"}"

"async function ottoAction(action,btn){"
"btn.style.borderColor='#7eb8ff';"
"const r=await api('/api/otto/action',{action});"
"setTimeout(()=>btn.style.borderColor='',1000);"
"setStatus('otto-status',r&&r.ok,r?(r.ok?'动作执行: '+action:r.msg):'失败');"
"}"

"async function saveAll(){"
"setStatus('main-status',true,'正在保存并重启...');"
"const r=await api('/api/setup/complete',{});"
"setStatus('main-status',r&&r.ok,r?(r.msg||'完成'):'失败');"
"}"

"async function factoryReset(){"
"if(!confirm('确定要清空所有配置并初始化设备吗？这个操作不可撤销！'))return;"
"const r=await api('/api/setup/reset',{});"
"setStatus('main-status',r&&r.ok,r?(r.msg||'重置中'):'失败');"
"}"

"loadStatus();"
"</script></body></html>";

static esp_err_t handle_setup_html(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, SETUP_HTML, strlen(SETUP_HTML));
}

/* ------------------------------------------------------------------ */
/*  HTTP server setup                                                  */
/* ------------------------------------------------------------------ */
#define URI(m, p, h)  { .uri = p, .method = m, .handler = h, .user_ctx = NULL }

static const httpd_uri_t s_uris[] = {
    URI(HTTP_GET,  "/",                    handle_root),
    URI(HTTP_GET,  "/setup.html",          handle_setup_html),
    URI(HTTP_GET,  "/api/status",          handle_status),
    URI(HTTP_GET,  "/api/wifi/scan",       handle_wifi_scan),
    URI(HTTP_POST, "/api/wifi/connect",    handle_wifi_connect),
    URI(HTTP_POST, "/api/llm/config",      handle_llm_config),
    URI(HTTP_POST, "/api/dingtalk/config", handle_dingtalk_config),
    URI(HTTP_POST, "/api/proxy/config",    handle_proxy_config),
    URI(HTTP_POST, "/api/search/config",   handle_search_config),
    URI(HTTP_POST, "/api/otto/action",     handle_otto_action),
    URI(HTTP_POST, "/api/setup/complete",  handle_setup_complete),
    URI(HTTP_POST, "/api/setup/reset",     handle_setup_reset),
};

static esp_err_t start_http_server(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port    = 80;
    cfg.max_uri_handlers = 16;
    cfg.recv_wait_timeout = 10;
    cfg.send_wait_timeout = 10;

    ESP_LOGI(TAG, "Starting HTTP server on port 80");
    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    for (int i = 0; i < (int)(sizeof(s_uris)/sizeof(s_uris[0])); i++) {
        httpd_register_uri_handler(s_server, &s_uris[i]);
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  AP mode WiFi                                                       */
/* ------------------------------------------------------------------ */
static esp_err_t start_ap(void)
{
    /* Build SSID from MAC */
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "OttoClaw-%02X%02X", mac[4], mac[5]);

    ESP_LOGI(TAG, "Starting AP: %s", s_ap_ssid);

    if (!s_ap_netif) {
        s_ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
        if (!s_ap_netif) {
            s_ap_netif = esp_netif_create_default_wifi_ap();
        }
    }
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (!s_sta_netif) {
            s_sta_netif = esp_netif_create_default_wifi_sta();
        }
    }

    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_get_mode failed: %s", esp_err_to_name(err));
    }
    if (mode != WIFI_MODE_NULL) {
        err = esp_wifi_stop();
        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_WIFI_NOT_STARTED) {
            ESP_LOGW(TAG, "esp_wifi_stop failed: %s", esp_err_to_name(err));
        }
        vTaskDelay(pdMS_TO_TICKS(300));
    }

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid_len    = 0,
            .channel     = 6,
            .authmode    = WIFI_AUTH_OPEN,
            .max_connection = 4,
        },
    };
    wifi_config_t sta_cfg = {0};
    strncpy((char *)ap_cfg.ap.ssid, s_ap_ssid, sizeof(ap_cfg.ap.ssid) - 1);
    sta_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
    sta_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP started. IP: 192.168.4.1");
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/*  Main portal task                                                   */
/* ------------------------------------------------------------------ */
static void portal_task(void *arg)
{
    /* Start AP */
    if (start_ap() != ESP_OK) {
        ESP_LOGE(TAG, "AP start failed");
        vTaskDelete(NULL);
        return;
    }

    /* Show setup instructions on LCD */
    lcd_set_state(LCD_STATE_CONFIG);
    char portal_hint[128];
    snprintf(portal_hint, sizeof(portal_hint),
             "热点: %s\n访问 http://192.168.4.1 进入web配置面板",
             s_ap_ssid[0] ? s_ap_ssid : "OttoClaw");
    lcd_show_qr_overlay("http://192.168.4.1", portal_hint);

    /* Start HTTP */
    if (start_http_server() != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed");
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Config portal ready at http://192.168.4.1");

    /* Wait until complete or reset is triggered */
    xEventGroupWaitBits(s_done_eg, PORTAL_DONE_BIT, pdFALSE, pdFALSE, portMAX_DELAY);

    /* Cleanup */
    ESP_LOGI(TAG, "Portal done — rebooting");
    lcd_hide_qr_overlay();
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

/* ------------------------------------------------------------------ */
/*  Public API                                                         */
/* ------------------------------------------------------------------ */
esp_err_t config_portal_start(void)
{
    if (s_running) return ESP_OK;

    s_done_eg = xEventGroupCreate();
    if (!s_done_eg) return ESP_ERR_NO_MEM;

    s_running = true;

    BaseType_t ret = xTaskCreatePinnedToCore(
        portal_task, "config_portal",
        8 * 1024, NULL, 5, NULL, 0);

    if (ret != pdPASS) {
        s_running = false;
        return ESP_FAIL;
    }
    return ESP_OK;
}

void config_portal_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    esp_wifi_stop();
    s_running = false;
}

bool config_portal_is_running(void)
{
    return s_running;
}
