#include "session_mgr.h"
#include "mimi_config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "session";

#define SESSION_METADATA_PREFIX "# SESSION_METADATA"

static void session_path(const char *chat_id, char *buf, size_t size)
{
    snprintf(buf, size, "%s/tg_%s.jsonl", MIMI_SPIFFS_SESSION_DIR, chat_id);
}

static esp_err_t session_write_metadata(FILE *f, time_t created_at, time_t updated_at)
{
    fprintf(f, "%s\n", SESSION_METADATA_PREFIX);
    fprintf(f, "{\"created_at\":%ld,\"updated_at\":%ld}\n", (long)created_at, (long)updated_at);
    return ESP_OK;
}

static esp_err_t session_update_metadata(const char *path)
{
    FILE *f = fopen(path, "r+");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    char line[512];
    time_t created_at = 0;
    time_t updated_at = time(NULL);
    bool has_metadata = false;
    long metadata_pos = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, SESSION_METADATA_PREFIX, strlen(SESSION_METADATA_PREFIX)) == 0) {
            has_metadata = true;
            if (fgets(line, sizeof(line), f)) {
                cJSON *obj = cJSON_Parse(line);
                if (obj) {
                    cJSON *created = cJSON_GetObjectItem(obj, "created_at");
                    if (created && cJSON_IsNumber(created)) {
                        created_at = (time_t)created->valuedouble;
                    }
                    cJSON_Delete(obj);
                }
            }
            metadata_pos = ftell(f);
            break;
        }
    }

    if (has_metadata) {
        fseek(f, metadata_pos, SEEK_SET);
        session_write_metadata(f, created_at, updated_at);
    } else {
        fclose(f);
        f = fopen(path, "r+");
        if (!f) {
            return ESP_ERR_NOT_FOUND;
        }
        
        char *content = NULL;
        long file_size = 0;
        
        fseek(f, 0, SEEK_END);
        file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        if (file_size > 0) {
            content = malloc(file_size + 1);
            if (content) {
                fread(content, 1, file_size, f);
                content[file_size] = '\0';
            }
        }
        
        fseek(f, 0, SEEK_SET);
        session_write_metadata(f, updated_at, updated_at);
        
        if (content) {
            fprintf(f, "%s", content);
            free(content);
        }
    }

    fclose(f);
    return ESP_OK;
}

esp_err_t session_mgr_init(void)
{
    DIR *dir = opendir(MIMI_SPIFFS_SESSION_DIR);
    if (!dir) {
        ESP_LOGI(TAG, "Session directory does not exist, will be created on first use");
    } else {
        closedir(dir);
    }
    
    ESP_LOGI(TAG, "Session manager initialized at %s", MIMI_SPIFFS_SESSION_DIR);
    return ESP_OK;
}

esp_err_t session_append(const char *chat_id, const char *role, const char *content)
{
    char path[128];
    char dir_path[64];
    snprintf(dir_path, sizeof(dir_path), "%s", MIMI_SPIFFS_SESSION_DIR);
    
    DIR *dir = opendir(dir_path);
    if (!dir) {
        mkdir(dir_path, 0777);
    } else {
        closedir(dir);
    }
    
    session_path(chat_id, path, sizeof(path));

    bool new_session = (access(path, F_OK) != 0);
    
    FILE *f = fopen(path, "a");
    if (!f) {
        ESP_LOGE(TAG, "Cannot open session file %s", path);
        return ESP_FAIL;
    }

    if (new_session) {
        time_t now = time(NULL);
        session_write_metadata(f, now, now);
    }

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "role", role);
    cJSON_AddStringToObject(obj, "content", content);
    cJSON_AddNumberToObject(obj, "ts", (double)time(NULL));

    char *line = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);

    if (line) {
        fprintf(f, "%s\n", line);
        free(line);
    }

    fclose(f);

    session_update_metadata(path);
    
    return ESP_OK;
}

esp_err_t session_get_history_json(const char *chat_id, char *buf, size_t size, int max_msgs)
{
    char path[128];
    session_path(chat_id, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(buf, size, "[]");
        return ESP_OK;
    }

    cJSON *messages[MIMI_SESSION_MAX_MSGS];
    int count = 0;
    int write_idx = 0;

    char line[2048];
    bool skip_metadata = false;
    
    while (fgets(line, sizeof(line), f)) {
        if (skip_metadata) {
            skip_metadata = false;
            continue;
        }
        
        if (strncmp(line, SESSION_METADATA_PREFIX, strlen(SESSION_METADATA_PREFIX)) == 0) {
            skip_metadata = true;
            continue;
        }

        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] == '\0') continue;

        cJSON *obj = cJSON_Parse(line);
        if (!obj) continue;

        if (count >= max_msgs) {
            cJSON_Delete(messages[write_idx]);
        }
        messages[write_idx] = obj;
        write_idx = (write_idx + 1) % max_msgs;
        if (count < max_msgs) count++;
    }
    fclose(f);

    cJSON *arr = cJSON_CreateArray();
    int start = (count < max_msgs) ? 0 : write_idx;
    for (int i = 0; i < count; i++) {
        int idx = (start + i) % max_msgs;
        cJSON *src = messages[idx];

        cJSON *entry = cJSON_CreateObject();
        cJSON *role = cJSON_GetObjectItem(src, "role");
        cJSON *content = cJSON_GetObjectItem(src, "content");
        if (role && content) {
            cJSON_AddStringToObject(entry, "role", role->valuestring);
            cJSON_AddStringToObject(entry, "content", content->valuestring);
        }
        cJSON_AddItemToArray(arr, entry);
    }

    int cleanup_start = (count < max_msgs) ? 0 : write_idx;
    for (int i = 0; i < count; i++) {
        int idx = (cleanup_start + i) % max_msgs;
        cJSON_Delete(messages[idx]);
    }

    char *json_str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (json_str) {
        strncpy(buf, json_str, size - 1);
        buf[size - 1] = '\0';
        free(json_str);
    } else {
        snprintf(buf, size, "[]");
    }

    return ESP_OK;
}

esp_err_t session_clear(const char *chat_id)
{
    char path[64];
    session_path(chat_id, path, sizeof(path));

    if (remove(path) == 0) {
        ESP_LOGI(TAG, "Session %s cleared", chat_id);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}

void session_list(void)
{
    DIR *dir = opendir(MIMI_SPIFFS_SESSION_DIR);
    if (!dir) {
        dir = opendir(MIMI_SPIFFS_BASE);
        if (!dir) {
            ESP_LOGW(TAG, "Cannot open SPIFFS directory");
            return;
        }
    }

    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, "tg_") && strstr(entry->d_name, ".jsonl")) {
            ESP_LOGI(TAG, "  Session: %s", entry->d_name);
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        ESP_LOGI(TAG, "  No sessions found");
    }
}

esp_err_t session_get_metadata(const char *chat_id, time_t *created_at, time_t *updated_at)
{
    char path[128];
    session_path(chat_id, path, sizeof(path));

    FILE *f = fopen(path, "r");
    if (!f) {
        return ESP_ERR_NOT_FOUND;
    }

    char line[512];
    bool found_metadata = false;
    
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, SESSION_METADATA_PREFIX, strlen(SESSION_METADATA_PREFIX)) == 0) {
            if (fgets(line, sizeof(line), f)) {
                cJSON *obj = cJSON_Parse(line);
                if (obj) {
                    cJSON *created = cJSON_GetObjectItem(obj, "created_at");
                    cJSON *updated = cJSON_GetObjectItem(obj, "updated_at");
                    
                    if (created && cJSON_IsNumber(created) && created_at) {
                        *created_at = (time_t)created->valuedouble;
                    }
                    
                    if (updated && cJSON_IsNumber(updated) && updated_at) {
                        *updated_at = (time_t)updated->valuedouble;
                    }
                    
                    cJSON_Delete(obj);
                    found_metadata = true;
                }
            }
            break;
        }
    }
    
    fclose(f);
    
    if (!found_metadata) {
        return ESP_ERR_NOT_FOUND;
    }
    
    return ESP_OK;
}

esp_err_t session_get_info_json(const char *chat_id, char *buf, size_t size)
{
    time_t created_at = 0;
    time_t updated_at = 0;
    
    if (session_get_metadata(chat_id, &created_at, &updated_at) != ESP_OK) {
        snprintf(buf, size, "{}");
        return ESP_OK;
    }
    
    cJSON *info = cJSON_CreateObject();
    cJSON_AddStringToObject(info, "chat_id", chat_id);
    cJSON_AddNumberToObject(info, "created_at", (double)created_at);
    cJSON_AddNumberToObject(info, "updated_at", (double)updated_at);
    
    char *json_str = cJSON_PrintUnformatted(info);
    cJSON_Delete(info);
    
    if (json_str) {
        strncpy(buf, json_str, size - 1);
        buf[size - 1] = '\0';
        free(json_str);
    } else {
        snprintf(buf, size, "{}");
    }
    
    return ESP_OK;
}
