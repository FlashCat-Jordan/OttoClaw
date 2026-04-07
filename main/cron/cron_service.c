#include "cron_service.h"
#include "mimi_config.h"
#include "bus/message_bus.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "cron";

#define MAX_CRON_TASKS 5
#define MAX_TASK_NAME 64
#define MAX_TASK_PROMPT 256

typedef struct {
    char name[MAX_TASK_NAME];
    char prompt[MAX_TASK_PROMPT];
    int interval_minutes;
    TimerHandle_t timer;
    bool active;
} cron_task_t;

static cron_task_t cron_tasks[MAX_CRON_TASKS];
static int task_count = 0;
static bool service_running = false;

static void cron_timer_callback(TimerHandle_t timer)
{
    cron_task_t *task = (cron_task_t *)pvTimerGetTimerID(timer);
    if (!task || !task->active) {
        return;
    }

    ESP_LOGI(TAG, "Executing cron task: %s", task->name);

    mimi_msg_t msg;
    strncpy(msg.channel, MIMI_CHAN_CRON, sizeof(msg.channel) - 1);
    strncpy(msg.chat_id, "system", sizeof(msg.chat_id) - 1);
    
    size_t prompt_len = strlen(task->prompt);
    msg.content = malloc(prompt_len + 1);
    if (msg.content) {
        strcpy(msg.content, task->prompt);
        message_bus_push_inbound(&msg);
        ESP_LOGI(TAG, "Cron task queued: %s", task->name);
    } else {
        ESP_LOGE(TAG, "Failed to allocate memory for cron task: %s", task->name);
    }
}

static esp_err_t parse_cron_file(void)
{
    FILE *f = fopen("/spiffs/config/CRON.md", "r");
    if (!f) {
        ESP_LOGW(TAG, "CRON.md not found, no scheduled tasks");
        return ESP_ERR_NOT_FOUND;
    }

    char line[512];
    task_count = 0;

    while (fgets(line, sizeof(line), f) != NULL && task_count < MAX_CRON_TASKS) {
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        if (strlen(trimmed) < 5 || trimmed[0] == '#') {
            continue;
        }

        char *colon = strchr(trimmed, ':');
        if (!colon) {
            continue;
        }

        *colon = '\0';
        char *name = trimmed;
        char *value = colon + 1;

        while (*value == ' ') value++;

        if (strncmp(name, "interval", 8) == 0) {
            int interval = atoi(value);
            if (interval > 0 && interval <= 1440) {
                cron_tasks[task_count].interval_minutes = interval;
            }
        } else if (strncmp(name, "task", 4) == 0) {
            strncpy(cron_tasks[task_count].prompt, value, MAX_TASK_PROMPT - 1);
            cron_tasks[task_count].prompt[MAX_TASK_PROMPT - 1] = '\0';
            
            size_t len = strlen(cron_tasks[task_count].prompt);
            if (len > 0 && cron_tasks[task_count].prompt[len - 1] == '\n') {
                cron_tasks[task_count].prompt[len - 1] = '\0';
            }
        } else if (strncmp(name, "name", 4) == 0) {
            strncpy(cron_tasks[task_count].name, value, MAX_TASK_NAME - 1);
            cron_tasks[task_count].name[MAX_TASK_NAME - 1] = '\0';
            
            size_t len = strlen(cron_tasks[task_count].name);
            if (len > 0 && cron_tasks[task_count].name[len - 1] == '\n') {
                cron_tasks[task_count].name[len - 1] = '\0';
            }
        } else if (strcmp(name, "---") == 0) {
            if (cron_tasks[task_count].interval_minutes > 0 && 
                cron_tasks[task_count].prompt[0] != '\0') {
                task_count++;
            }
        }
    }

    fclose(f);

    if (cron_tasks[task_count].interval_minutes > 0 && 
        cron_tasks[task_count].prompt[0] != '\0') {
        task_count++;
    }

    ESP_LOGI(TAG, "Loaded %d cron tasks from CRON.md", task_count);
    return ESP_OK;
}

static void create_timers(void)
{
    for (int i = 0; i < task_count; i++) {
        if (cron_tasks[i].interval_minutes <= 0) {
            continue;
        }

        char timer_name[32];
        snprintf(timer_name, sizeof(timer_name), "cron_%d", i);

        uint64_t period_ms = cron_tasks[i].interval_minutes * 60 * 1000;
        
        cron_tasks[i].timer = xTimerCreate(
            timer_name,
            pdMS_TO_TICKS(period_ms),
            pdTRUE,
            &cron_tasks[i],
            cron_timer_callback
        );

        if (cron_tasks[i].timer) {
            ESP_LOGI(TAG, "Created timer for task '%s': every %d minutes",
                     cron_tasks[i].name, cron_tasks[i].interval_minutes);
        } else {
            ESP_LOGE(TAG, "Failed to create timer for task '%s'", cron_tasks[i].name);
        }
    }
}

static void start_timers(void)
{
    for (int i = 0; i < task_count; i++) {
        if (cron_tasks[i].timer && !cron_tasks[i].active) {
            if (xTimerStart(cron_tasks[i].timer, 0) == pdPASS) {
                cron_tasks[i].active = true;
                ESP_LOGI(TAG, "Started timer for task '%s'", cron_tasks[i].name);
            } else {
                ESP_LOGE(TAG, "Failed to start timer for task '%s'", cron_tasks[i].name);
            }
        }
    }
}

static void stop_timers(void)
{
    for (int i = 0; i < task_count; i++) {
        if (cron_tasks[i].timer && cron_tasks[i].active) {
            if (xTimerStop(cron_tasks[i].timer, 0) == pdPASS) {
                cron_tasks[i].active = false;
                ESP_LOGI(TAG, "Stopped timer for task '%s'", cron_tasks[i].name);
            }
        }
    }
}

static void delete_timers(void)
{
    for (int i = 0; i < task_count; i++) {
        if (cron_tasks[i].timer) {
            xTimerDelete(cron_tasks[i].timer, 0);
            cron_tasks[i].timer = NULL;
            cron_tasks[i].active = false;
        }
    }
}

esp_err_t cron_service_init(void)
{
    task_count = 0;
    service_running = false;
    memset(cron_tasks, 0, sizeof(cron_tasks));

    esp_err_t ret = parse_cron_file();
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "No cron tasks loaded, service will be idle");
    }

    create_timers();
    return ESP_OK;
}

esp_err_t cron_service_start(void)
{
    if (service_running) {
        ESP_LOGW(TAG, "Cron service already running");
        return ESP_OK;
    }

    start_timers();
    service_running = true;
    ESP_LOGI(TAG, "Cron service started");
    return ESP_OK;
}

esp_err_t cron_service_stop(void)
{
    if (!service_running) {
        ESP_LOGW(TAG, "Cron service not running");
        return ESP_OK;
    }

    stop_timers();
    service_running = false;
    ESP_LOGI(TAG, "Cron service stopped");
    return ESP_OK;
}

bool cron_service_is_running(void)
{
    return service_running;
}

esp_err_t cron_service_reload(void)
{
    bool was_running = service_running;
    
    if (was_running) {
        cron_service_stop();
    }

    stop_timers();
    delete_timers();

    esp_err_t ret = cron_service_init();
    if (ret != ESP_OK) {
        return ret;
    }

    if (was_running) {
        return cron_service_start();
    }

    return ESP_OK;
}
