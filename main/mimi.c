#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "mimi_config.h"
#include "bus/message_bus.h"
#include "wifi/wifi_manager.h"
#include "dingtalk/dingtalk_bot.h"
#include "llm/llm_proxy.h"
#include "agent/agent_loop.h"
#include "memory/memory_store.h"
#include "memory/session_mgr.h"
#include "gateway/ws_server.h"
#include "cli/serial_cli.h"
#include "proxy/http_proxy.h"
#include "tools/tool_registry.h"
#include "lcd/lcd_display.h"
#include "skills/skills.h"
#include "cron/cron_service.h"
#include "heartbeat/heartbeat_service.h"
#include "voice/voice_transcription.h"
#include "otto/otto_movements.h"
#include "config_portal/config_portal.h"

static const char *TAG = "mimi";

otto_t g_otto;

static const int OTTO_LEFT_LEG   = 17;
static const int OTTO_RIGHT_LEG  = 39;
static const int OTTO_LEFT_FOOT  = 18;
static const int OTTO_RIGHT_FOOT = 38;
static const int OTTO_LEFT_HAND  = 8;
static const int OTTO_RIGHT_HAND = 12;

/* BOOT button — GPIO 0 on most ESP32-S3 devkits */
#define BOOT_BUTTON_GPIO  GPIO_NUM_0
#define LONG_PRESS_MS     2000

static esp_err_t init_nvs(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

static esp_err_t init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = MIMI_SPIFFS_BASE,
        .partition_label = NULL,
        .max_files = 10,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS: total=%d, used=%d", (int)total, (int)used);

    return ESP_OK;
}

/* Outbound dispatch task: reads from outbound queue and routes to channels */
static void outbound_dispatch_task(void *arg)
{
    ESP_LOGI(TAG, "Outbound dispatch started");

    while (1) {
        mimi_msg_t msg;
        if (message_bus_pop_outbound(&msg, UINT32_MAX) != ESP_OK) continue;

        ESP_LOGI(TAG, "Dispatching response to %s:%s", msg.channel, msg.chat_id);

        if (strcmp(msg.channel, MIMI_CHAN_DINGTALK) == 0) {
            dingtalk_send_message(msg.chat_id, msg.content);
        } else if (strcmp(msg.channel, MIMI_CHAN_WEBSOCKET) == 0) {
            ws_server_send(msg.chat_id, msg.content);
        } else {
            ESP_LOGW(TAG, "Unknown channel: %s", msg.channel);
        }

        message_bus_free_msg(&msg);
    }
}

/* Check if BOOT button has been held for LONG_PRESS_MS */
static bool boot_button_long_press_detected(void)
{
    if (gpio_get_level(BOOT_BUTTON_GPIO) != 0) return false;  /* not pressed */

    /* Wait to see if it stays low for the full duration */
    uint32_t start = xTaskGetTickCount();
    while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
        if ((xTaskGetTickCount() - start) * portTICK_PERIOD_MS >= LONG_PRESS_MS) {
            ESP_LOGI(TAG, "BOOT button long press detected — entering config portal");
            return true;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return false;
}

static void start_normal_services(void)
{
    ESP_ERROR_CHECK(dingtalk_bot_start());
    ESP_ERROR_CHECK(agent_loop_start());
    ESP_ERROR_CHECK(ws_server_start());
    ESP_ERROR_CHECK(cron_service_start());
    ESP_ERROR_CHECK(heartbeat_service_start());

    /* Outbound dispatch task */
    xTaskCreatePinnedToCore(
        outbound_dispatch_task, "outbound",
        MIMI_OUTBOUND_STACK, NULL,
        MIMI_OUTBOUND_PRIO, NULL, MIMI_OUTBOUND_CORE);

    ESP_LOGI(TAG, "All services started!");
}

void app_main(void)
{
    /* Silence noisy components */
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  MimiClaw - ESP32-S3 AI Agent");
    ESP_LOGI(TAG, "========================================");

    /* Print memory info */
    ESP_LOGI(TAG, "Internal free: %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "PSRAM free:    %d bytes",
             (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    /* Phase 1: Core infrastructure */
    ESP_ERROR_CHECK(init_nvs());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(init_spiffs());

    /* Initialize subsystems */
    ESP_ERROR_CHECK(message_bus_init());
    ESP_ERROR_CHECK(memory_store_init());
    ESP_ERROR_CHECK(session_mgr_init());
    ESP_ERROR_CHECK(skills_init());
    ESP_ERROR_CHECK(cron_service_init());
    ESP_ERROR_CHECK(heartbeat_service_init());
    ESP_ERROR_CHECK(voice_transcription_init());
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(http_proxy_init());
    ESP_ERROR_CHECK(dingtalk_bot_init());
    ESP_ERROR_CHECK(llm_proxy_init());
    ESP_ERROR_CHECK(tool_registry_init());
    ESP_ERROR_CHECK(agent_loop_init());

    /* Start Serial CLI first (works without WiFi) */
    ESP_ERROR_CHECK(serial_cli_init());

    /* Initialize LCD display */
    ESP_LOGI(TAG, "Initializing LCD...");
    esp_err_t lcd_err = lcd_display_init();
    if (lcd_err == ESP_OK) {
        ESP_LOGI(TAG, "LCD initialized successfully");
    } else {
        ESP_LOGW(TAG, "LCD initialization failed: %s", esp_err_to_name(lcd_err));
    }

    /* Initialize Otto robot */
    ESP_LOGI(TAG, "Initializing Otto robot...");
    otto_init(&g_otto, OTTO_LEFT_LEG, OTTO_RIGHT_LEG, OTTO_LEFT_FOOT, OTTO_RIGHT_FOOT, OTTO_LEFT_HAND, OTTO_RIGHT_HAND);
    otto_home(&g_otto, true);
    ESP_LOGI(TAG, "Otto robot initialized successfully");

    /* Configure BOOT button as input */
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    /* ── Decide: Config portal or normal STA mode ── */
    bool need_config = false;

    /* Check if BOOT button is held at power-on */
    if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
        ESP_LOGI(TAG, "BOOT button held at startup — entering config portal");
        need_config = true;
    }

    /* Check if WiFi credentials are available */
    if (!need_config && !wifi_manager_has_saved_credentials()) {
        ESP_LOGI(TAG, "No WiFi credentials found — entering config portal");
        need_config = true;
    }

    if (need_config) {
        /* Start config portal (AP mode + web UI) — this does NOT return.
         * The portal task will call esp_restart() when done. */
        ESP_LOGI(TAG, "Starting config portal...");
        lcd_set_state(LCD_STATE_CONFIG);
        config_portal_start();

        /* Block here — portal task handles the rest and calls esp_restart() */
        while (1) {
            /* Poll BOOT button — not needed here since portal handles it */
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        return;
    }

    /* Normal startup: connect to WiFi */
    esp_err_t wifi_err = wifi_manager_start_from_nvs();
    if (wifi_err == ESP_OK) {
        lcd_set_state(LCD_STATE_CONNECTING);
        ESP_LOGI(TAG, "Waiting for WiFi connection...");

        if (wifi_manager_wait_connected(40000) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());
            lcd_set_state(LCD_STATE_CONNECTED);

            /* Start network-dependent services */
            start_normal_services();

            vTaskDelay(pdMS_TO_TICKS(3000));
            lcd_set_state(LCD_STATE_SLEEPING);
        } else {
            ESP_LOGW(TAG, "WiFi connection timeout (40s).");
            lcd_set_state(LCD_STATE_ERROR);
            lcd_set_status_text("WiFi 超时");
        }
    } else {
        ESP_LOGW(TAG, "WiFi start failed: %s", esp_err_to_name(wifi_err));
        lcd_set_state(LCD_STATE_ERROR);
        lcd_set_status_text("无WiFi配置");
    }

    ESP_LOGI(TAG, "MimiClaw ready. Type 'help' for CLI commands.");

    /* Main loop: poll for BOOT long-press to enter config mode */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (boot_button_long_press_detected()) {
            ESP_LOGI(TAG, "Re-entering config portal via BOOT button");
            lcd_set_state(LCD_STATE_CONFIG);
            lcd_set_status_text("配置模式");
            /* Stop any running services gracefully (best-effort) */
            vTaskDelay(pdMS_TO_TICKS(500));
            config_portal_start();
            /* Block — portal will restart device */
            while (1) vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
