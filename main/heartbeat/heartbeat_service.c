#include "heartbeat_service.h"
#include "mimi_config.h"
#include "bus/message_bus.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "heartbeat";

#define HEARTBEAT_INTERVAL_MS (30 * 60 * 1000) /* 30 minutes */
#define HEARTBEAT_FILE "/spiffs/config/HEARTBEAT.md"

static TimerHandle_t heartbeat_timer = NULL;
static bool service_running = false;

static void heartbeat_timer_callback(TimerHandle_t timer)
{
    ESP_LOGI(TAG, "Heartbeat triggered, checking HEARTBEAT.md");
    heartbeat_service_trigger();
}

static bool has_tasks_in_heartbeat(void)
{
    FILE *f = fopen(HEARTBEAT_FILE, "r");
    if (!f) {
        ESP_LOGD(TAG, "HEARTBEAT.md not found");
        return false;
    }

    char line[512];
    bool has_tasks = false;

    while (fgets(line, sizeof(line), f) != NULL) {
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (strlen(trimmed) < 2 || trimmed[0] == '#') {
            continue;
        }

        if (trimmed[0] == '-' || trimmed[0] == '*') {
            has_tasks = true;
            break;
        }
    }

    fclose(f);
    return has_tasks;
}

esp_err_t heartbeat_service_init(void)
{
    service_running = false;

    char timer_name[] = "heartbeat";
    heartbeat_timer = xTimerCreate(
        timer_name,
        pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS),
        pdTRUE,
        NULL,
        heartbeat_timer_callback
    );

    if (!heartbeat_timer) {
        ESP_LOGE(TAG, "Failed to create heartbeat timer");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Heartbeat service initialized (interval: %d minutes)", 
             HEARTBEAT_INTERVAL_MS / 60000);
    return ESP_OK;
}

esp_err_t heartbeat_service_start(void)
{
    if (service_running) {
        ESP_LOGW(TAG, "Heartbeat service already running");
        return ESP_OK;
    }

    if (!heartbeat_timer) {
        ESP_LOGE(TAG, "Heartbeat timer not initialized");
        return ESP_FAIL;
    }

    if (xTimerStart(heartbeat_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to start heartbeat timer");
        return ESP_FAIL;
    }

    service_running = true;
    ESP_LOGI(TAG, "Heartbeat service started");
    return ESP_OK;
}

esp_err_t heartbeat_service_stop(void)
{
    if (!service_running) {
        ESP_LOGW(TAG, "Heartbeat service not running");
        return ESP_OK;
    }

    if (!heartbeat_timer) {
        ESP_LOGE(TAG, "Heartbeat timer not initialized");
        return ESP_FAIL;
    }

    if (xTimerStop(heartbeat_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "Failed to stop heartbeat timer");
        return ESP_FAIL;
    }

    service_running = false;
    ESP_LOGI(TAG, "Heartbeat service stopped");
    return ESP_OK;
}

bool heartbeat_service_is_running(void)
{
    return service_running;
}

esp_err_t heartbeat_service_trigger(void)
{
    if (!has_tasks_in_heartbeat()) {
        ESP_LOGD(TAG, "No tasks found in HEARTBEAT.md");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Tasks found in HEARTBEAT.md, triggering agent");

    mimi_msg_t msg;
    strncpy(msg.channel, MIMI_CHAN_HEARTBEAT, sizeof(msg.channel) - 1);
    strncpy(msg.chat_id, "system", sizeof(msg.chat_id) - 1);
    
    const char *prompt = "Please check HEARTBEAT.md for pending tasks and handle them accordingly.";
    msg.content = malloc(strlen(prompt) + 1);
    if (msg.content) {
        strcpy(msg.content, prompt);
        message_bus_push_inbound(&msg);
        ESP_LOGI(TAG, "Heartbeat task queued");
    } else {
        ESP_LOGE(TAG, "Failed to allocate memory for heartbeat task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}
