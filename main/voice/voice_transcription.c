#include "voice_transcription.h"
#include "mimi_config.h"
#include "proxy/http_proxy.h"

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "nvs.h"
#include "cJSON.h"

static const char *TAG = "voice";

#define WHISPER_API_URL "https://api.openai.com/v1/audio/transcriptions"
#define WHISPER_MODEL "whisper-1"
#define MAX_API_KEY 128
#define MAX_BASE_URL 256

static char s_api_key[MAX_API_KEY] = {0};
static char s_base_url[MAX_BASE_URL] = {0};

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} resp_buf_t;

static esp_err_t resp_buf_init(resp_buf_t *rb, size_t initial_cap)
{
    rb->data = heap_caps_calloc(1, initial_cap, MALLOC_CAP_SPIRAM);
    if (!rb->data) return ESP_ERR_NO_MEM;
    rb->len = 0;
    rb->cap = initial_cap;
    return ESP_OK;
}

static esp_err_t resp_buf_append(resp_buf_t *rb, const char *data, size_t len)
{
    while (rb->len + len >= rb->cap) {
        size_t new_cap = rb->cap * 2;
        char *tmp = heap_caps_realloc(rb->data, new_cap, MALLOC_CAP_SPIRAM);
        if (!tmp) return ESP_ERR_NO_MEM;
        rb->data = tmp;
        rb->cap = new_cap;
    }
    memcpy(rb->data + rb->len, data, len);
    rb->len += len;
    rb->data[rb->len] = '\0';
    return ESP_OK;
}

static void resp_buf_free(resp_buf_t *rb)
{
    free(rb->data);
    rb->data = NULL;
    rb->len = 0;
    rb->cap = 0;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    resp_buf_t *rb = (resp_buf_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        resp_buf_append(rb, (const char *)evt->data, evt->data_len);
    }
    return ESP_OK;
}

static esp_err_t transcribe_via_http(const uint8_t *audio_data, size_t audio_len,
                                    char *transcript_buf, size_t buf_size)
{
    if (!audio_data || audio_len == 0 || !transcript_buf || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_api_key[0] == '\0') {
        ESP_LOGE(TAG, "No API key configured");
        return ESP_ERR_INVALID_STATE;
    }

    const char *url = (s_base_url[0] != '\0') ? s_base_url : WHISPER_API_URL;

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = 120 * 1000,
        .buffer_size = 8192,
        .buffer_size_tx = 8192,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return ESP_FAIL;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW");

    char auth[256];
    snprintf(auth, sizeof(auth), "Bearer %s", s_api_key);
    esp_http_client_set_header(client, "Authorization", auth);

    resp_buf_t rb;
    if (resp_buf_init(&rb, 4096) != ESP_OK) {
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    char *post_data = NULL;
    size_t post_len = 0;

    char *boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    
    size_t header_len = snprintf(NULL, 0, 
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n"
        "\r\n",
        boundary);
    
    size_t footer_len = snprintf(NULL, 0, 
        "\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n"
        "\r\n"
        "%s\r\n"
        "--%s--\r\n",
        boundary, WHISPER_MODEL, boundary);
    
    post_len = header_len + audio_len + footer_len;
    
    post_data = malloc(post_len + 1);
    if (!post_data) {
        resp_buf_free(&rb);
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }

    size_t off = 0;
    off += snprintf(post_data + off, post_len - off, 
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"audio.wav\"\r\n"
        "Content-Type: audio/wav\r\n"
        "\r\n",
        boundary);
    
    memcpy(post_data + off, audio_data, audio_len);
    off += audio_len;
    
    snprintf(post_data + off, post_len - off, 
        "\r\n"
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"model\"\r\n"
        "\r\n"
        "%s\r\n"
        "--%s--\r\n",
        boundary, WHISPER_MODEL, boundary);

    esp_http_client_set_post_field(client, post_data, post_len);

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
        resp_buf_free(&rb);
        free(post_data);
        return err;
    }

    if (status != 200) {
        ESP_LOGE(TAG, "API returned status %d: %s", status, rb.data);
        resp_buf_free(&rb);
        free(post_data);
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(rb.data);
    resp_buf_free(&rb);
    free(post_data);

    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        return ESP_FAIL;
    }

    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (text && cJSON_IsString(text)) {
        strncpy(transcript_buf, text->valuestring, buf_size - 1);
        transcript_buf[buf_size - 1] = '\0';
        ESP_LOGI(TAG, "Transcription successful: %s", transcript_buf);
        cJSON_Delete(root);
        return ESP_OK;
    }

    cJSON *error = cJSON_GetObjectItem(root, "error");
    if (error) {
        cJSON *msg = cJSON_GetObjectItem(error, "message");
        if (msg && cJSON_IsString(msg)) {
            ESP_LOGE(TAG, "API error: %s", msg->valuestring);
        }
    }

    cJSON_Delete(root);
    return ESP_FAIL;
}

esp_err_t voice_transcription_init(void)
{
    nvs_handle_t nvs;
    esp_err_t err = nvs_open(MIMI_NVS_LLM, NVS_READONLY, &nvs);
    if (err == ESP_OK) {
        size_t len = sizeof(s_api_key);
        nvs_get_str(nvs, MIMI_NVS_KEY_API_KEY, s_api_key, &len);
        
        len = sizeof(s_base_url);
        nvs_get_str(nvs, MIMI_NVS_KEY_WHISPER_BASE_URL, s_base_url, &len);
        
        nvs_close(nvs);
    }

    if (s_api_key[0] != '\0') {
        ESP_LOGI(TAG, "Voice transcription initialized");
    } else {
        ESP_LOGW(TAG, "No API key configured for voice transcription");
    }

    return ESP_OK;
}

esp_err_t voice_transcribe_file(const char *audio_path, char *transcript_buf, size_t buf_size)
{
    if (!audio_path || !transcript_buf || buf_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    FILE *f = fopen(audio_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open audio file: %s", audio_path);
        return ESP_ERR_NOT_FOUND;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size <= 0 || file_size > 10 * 1024 * 1024) {
        ESP_LOGE(TAG, "Invalid audio file size: %ld", file_size);
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *audio_data = malloc(file_size);
    if (!audio_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for audio data");
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t read_len = fread(audio_data, 1, file_size, f);
    fclose(f);

    if (read_len != (size_t)file_size) {
        ESP_LOGE(TAG, "Failed to read complete audio file");
        free(audio_data);
        return ESP_FAIL;
    }

    esp_err_t err = transcribe_via_http(audio_data, read_len, transcript_buf, buf_size);
    free(audio_data);

    return err;
}

esp_err_t voice_transcribe_data(const uint8_t *audio_data, size_t audio_len,
                              char *transcript_buf, size_t buf_size)
{
    return transcribe_via_http(audio_data, audio_len, transcript_buf, buf_size);
}

esp_err_t voice_set_api_key(const char *api_key)
{
    if (!api_key) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_api_key, api_key, sizeof(s_api_key) - 1);
    s_api_key[sizeof(s_api_key) - 1] = '\0';

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(MIMI_NVS_LLM, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        nvs_set_str(nvs, MIMI_NVS_KEY_API_KEY, s_api_key);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    ESP_LOGI(TAG, "API key updated");
    return ESP_OK;
}

esp_err_t voice_set_base_url(const char *base_url)
{
    if (!base_url) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_base_url, base_url, sizeof(s_base_url) - 1);
    s_base_url[sizeof(s_base_url) - 1] = '\0';

    nvs_handle_t nvs;
    esp_err_t err = nvs_open(MIMI_NVS_LLM, NVS_READWRITE, &nvs);
    if (err == ESP_OK) {
        nvs_set_str(nvs, MIMI_NVS_KEY_WHISPER_BASE_URL, s_base_url);
        nvs_commit(nvs);
        nvs_close(nvs);
    }

    ESP_LOGI(TAG, "Base URL updated: %s", s_base_url);
    return ESP_OK;
}
