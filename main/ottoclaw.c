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

#include "ottoclaw_config.h"
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
#include "relation/relation.h"

static const char *TAG = "ottoclaw";

otto_t g_otto;

static const int OTTO_LEFT_LEG   = 17;
static const int OTTO_RIGHT_LEG  = 39;
static const int OTTO_LEFT_FOOT  = 18;
static const int OTTO_RIGHT_FOOT = 38;
static const int OTTO_LEFT_HAND  = 8;
static const int OTTO_RIGHT_HAND = 12;

/* BOOT button — GPIO 0 on most ESP32-S3 devkits — short press enters config portal */
#define BOOT_BUTTON_GPIO  GPIO_NUM_0

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
        .base_path = OTTOCLAW_SPIFFS_BASE,
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
        ottoclaw_msg_t msg;
        if (message_bus_pop_outbound(&msg, UINT32_MAX) != ESP_OK) continue;

        ESP_LOGI(TAG, "Dispatching response to %s:%s", msg.channel, msg.chat_id);

        if (strcmp(msg.channel, OTTOCLAW_CHAN_DINGTALK) == 0) {
            dingtalk_send_message(msg.chat_id, msg.content);
        } else if (strcmp(msg.channel, OTTOCLAW_CHAN_WEBSOCKET) == 0) {
            ws_server_send(msg.chat_id, msg.content);
        } else {
            ESP_LOGW(TAG, "Unknown channel: %s", msg.channel);
        }

        message_bus_free_msg(&msg);
    }
}

/* Check if BOOT button is short-pressed (low → debounce → release) */
static bool boot_button_pressed(void)
{
    if (gpio_get_level(BOOT_BUTTON_GPIO) != 0) return false;
    vTaskDelay(pdMS_TO_TICKS(30));      /* debounce */
    if (gpio_get_level(BOOT_BUTTON_GPIO) != 0) return false;  /* glitch */
    while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {           /* wait release */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return true;
}

static void start_normal_services(void)
{
    esp_err_t err;

    err = dingtalk_bot_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "dingtalk_bot_start failed: %s", esp_err_to_name(err));
    }

    err = agent_loop_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "agent_loop_start failed: %s", esp_err_to_name(err));
    }

    err = ws_server_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ws_server_start failed: %s", esp_err_to_name(err));
    }

    err = cron_service_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "cron_service_start failed: %s", esp_err_to_name(err));
    }

    err = heartbeat_service_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "heartbeat_service_start failed: %s", esp_err_to_name(err));
    }

    /* Outbound dispatch task */
    BaseType_t ret = xTaskCreatePinnedToCore(
        outbound_dispatch_task, "outbound",
        OTTOCLAW_OUTBOUND_STACK, NULL,
        OTTOCLAW_OUTBOUND_PRIO, NULL, OTTOCLAW_OUTBOUND_CORE);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "outbound_dispatch_task start failed");
    }

    ESP_LOGI(TAG, "Service startup attempted");
}

void app_main(void)
{
    /* Silence noisy components */
    esp_log_level_set("esp-x509-crt-bundle", ESP_LOG_WARN);

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  OttoClaw - 闪猫科技 ESP32-S3 AI Agent");
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

    /* Initialize LCD display early so screen lights up fast */
    ESP_LOGI(TAG, "Initializing LCD...");
    esp_err_t lcd_err = lcd_display_init();
    if (lcd_err == ESP_OK) {
        ESP_LOGI(TAG, "LCD initialized successfully");
        lcd_set_state(LCD_STATE_CONNECTING);
    } else {
        ESP_LOGW(TAG, "LCD initialization failed: %s", esp_err_to_name(lcd_err));
    }

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
    relation_init();
    lcd_update_hearts(relation_get_stage_level());

    /* Start Serial CLI first (works without WiFi) */
    ESP_ERROR_CHECK(serial_cli_init());

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

        /* Poll WiFi status and BOOT button simultaneously */
        bool wifi_connected = false;
        bool boot_pressed = false;
        uint32_t wifi_start = xTaskGetTickCount();
        const uint32_t WIFI_TIMEOUT_TICKS = pdMS_TO_TICKS(40000);

        while ((xTaskGetTickCount() - wifi_start) < WIFI_TIMEOUT_TICKS) {
            EventGroupHandle_t eg = wifi_manager_get_event_group();
            EventBits_t bits = xEventGroupGetBits(eg);
            if (bits & WIFI_CONNECTED_BIT) {
                wifi_connected = true;
                break;
            }
            if (bits & WIFI_FAIL_BIT) {
                break;
            }
            /* Allow short press BOOT to enter config portal during WiFi scan */
            if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                vTaskDelay(pdMS_TO_TICKS(30));
                if (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                    while (gpio_get_level(BOOT_BUTTON_GPIO) == 0) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    boot_pressed = true;
                    break;
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (wifi_connected) {
            ESP_LOGI(TAG, "WiFi connected: %s", wifi_manager_get_ip());
            lcd_set_state(LCD_STATE_CONNECTED);
            start_normal_services();

            /* Wait for network stack to stabilize before validation */
            vTaskDelay(pdMS_TO_TICKS(3000));

            /* Validate LLM API key */
            esp_err_t llm_err = llm_validate_key();
            if (llm_err == ESP_ERR_INVALID_STATE) {
                /* No API key configured */
                ESP_LOGW(TAG, "No LLM API key configured");
                lcd_set_state(LCD_STATE_ERROR);
                lcd_set_status_text("未配置大模型 · 短按BOOT修改");
            } else if (llm_err == ESP_ERR_INVALID_RESPONSE) {
                int status = llm_get_last_http_status();
                if (status == 401) {
                    ESP_LOGE(TAG, "LLM API key invalid (401)");
                    lcd_set_state(LCD_STATE_ERROR);
                    lcd_set_status_text("大模型密钥错误 · 短按BOOT修改");
                } else if (status == 400) {
                    ESP_LOGE(TAG, "LLM model name invalid (400)");
                    lcd_set_state(LCD_STATE_ERROR);
                    lcd_set_status_text("模型名称错误 · 短按BOOT修改");
                } else {
                    ESP_LOGE(TAG, "LLM auth error (status=%d)", status);
                    lcd_set_state(LCD_STATE_ERROR);
                    lcd_set_status_text("大模型认证失败 · 短按BOOT修改");
                }
            } else if (llm_err == ESP_FAIL) {
                ESP_LOGW(TAG, "LLM network error — will retry on first chat");
            }

            /* Check DingTalk credential status after delay */
            vTaskDelay(pdMS_TO_TICKS(8000));
            dingtalk_status_t dt_status = dingtalk_get_status();
            if (dt_status == DT_NO_CREDS) {
                ESP_LOGW(TAG, "DingTalk not configured");
                if (llm_err == ESP_OK) {
                    lcd_set_state(LCD_STATE_ERROR);
                    lcd_set_status_text("钉钉未配置 · 短按BOOT修改");
                }
            } else if (dt_status == DT_TOKEN_FAIL) {
                ESP_LOGE(TAG, "DingTalk credentials invalid");
                if (llm_err == ESP_OK) {
                    lcd_set_state(LCD_STATE_ERROR);
                    lcd_set_status_text("钉钉密钥错误 · 短按BOOT修改");
                }
            }

            /* If no errors, go to sleeping */
            if (llm_err == ESP_OK && (dt_status == DT_OK || dt_status == DT_WS_FAIL)) {
                vTaskDelay(pdMS_TO_TICKS(3000));
                lcd_set_state(LCD_STATE_SLEEPING);
            }
        } else if (boot_pressed) {
            ESP_LOGI(TAG, "BOOT pressed during WiFi scan — entering config portal");
            lcd_set_state(LCD_STATE_CONFIG);
            lcd_set_status_text("配置模式");
            vTaskDelay(pdMS_TO_TICKS(500));
            config_portal_start();
            while (1) vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            /* WiFi failed — auto-enter config portal with specific reason */
            wifi_err_reason_t reason = wifi_manager_get_last_reason();
            const char *wifi_err_msg;
            if (reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
                wifi_err_msg = "WiFi密码错误 · 短按BOOT修改";
            } else if (reason == WIFI_REASON_NO_AP_FOUND) {
                wifi_err_msg = "WiFi找不到 · 检查SSID · 短按BOOT修改";
            } else if (reason == WIFI_REASON_CONNECTION_FAIL) {
                wifi_err_msg = "WiFi连接失败 · 短按BOOT修改";
            } else {
                wifi_err_msg = "WiFi超时 · 短按BOOT修改";
            }
            ESP_LOGW(TAG, "WiFi connection failed (reason=%d) — auto-entering config portal", reason);
            lcd_set_state(LCD_STATE_CONFIG);
            lcd_set_status_text(wifi_err_msg);
            vTaskDelay(pdMS_TO_TICKS(500));
            config_portal_start();
            while (1) vTaskDelay(pdMS_TO_TICKS(1000));
        }
    } else {
        /* WiFi start failed — auto-enter config portal */
        ESP_LOGW(TAG, "WiFi start failed: %s — entering config portal", esp_err_to_name(wifi_err));
        lcd_set_state(LCD_STATE_CONFIG);
        lcd_set_status_text("无WiFi配置 · 进入配网");
        vTaskDelay(pdMS_TO_TICKS(500));
        config_portal_start();
        while (1) vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "OttoClaw ready. Type 'help' for CLI commands.");

    /* Main loop: short press BOOT to re-enter config portal */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (boot_button_pressed()) {
            ESP_LOGI(TAG, "Re-entering config portal via BOOT button");
            lcd_set_state(LCD_STATE_CONFIG);
            lcd_set_status_text("配置模式");
            vTaskDelay(pdMS_TO_TICKS(500));
            config_portal_start();
            while (1) vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
