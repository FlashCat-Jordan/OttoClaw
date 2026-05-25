#include "message_bus.h"
#include "ottoclaw_config.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "bus";

static QueueHandle_t s_inbound_queue;
static QueueHandle_t s_outbound_queue;

esp_err_t message_bus_init(void)
{
    s_inbound_queue = xQueueCreate(OTTOCLAW_BUS_QUEUE_LEN, sizeof(ottoclaw_msg_t));
    s_outbound_queue = xQueueCreate(OTTOCLAW_BUS_QUEUE_LEN, sizeof(ottoclaw_msg_t));

    if (!s_inbound_queue || !s_outbound_queue) {
        ESP_LOGE(TAG, "Failed to create message queues");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Message bus initialized (queue depth %d)", OTTOCLAW_BUS_QUEUE_LEN);
    return ESP_OK;
}

esp_err_t message_bus_push_inbound(const ottoclaw_msg_t *msg)
{
    if (xQueueSend(s_inbound_queue, msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Inbound queue full, dropping message");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t message_bus_pop_inbound(ottoclaw_msg_t *msg, uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xQueueReceive(s_inbound_queue, msg, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

esp_err_t message_bus_push_outbound(const ottoclaw_msg_t *msg)
{
    if (xQueueSend(s_outbound_queue, msg, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGW(TAG, "Outbound queue full, dropping message");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t message_bus_pop_outbound(ottoclaw_msg_t *msg, uint32_t timeout_ms)
{
    TickType_t ticks = (timeout_ms == UINT32_MAX) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    if (xQueueReceive(s_outbound_queue, msg, ticks) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    return ESP_OK;
}

void message_bus_free_msg(ottoclaw_msg_t *msg)
{
    if (!msg) return;
    if (msg->content) {
        free(msg->content);
        msg->content = NULL;
    }
    if (msg->media_path) {
        free(msg->media_path);
        msg->media_path = NULL;
    }
    if (msg->reply_to) {
        free(msg->reply_to);
        msg->reply_to = NULL;
    }
    if (msg->metadata) {
        free(msg->metadata);
        msg->metadata = NULL;
    }
}
