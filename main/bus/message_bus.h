#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

/* Channel identifiers */
#define OTTOCLAW_CHAN_DINGTALK   "dingtalk"
#define OTTOCLAW_CHAN_WEBSOCKET  "websocket"
#define OTTOCLAW_CHAN_CLI        "cli"
#define OTTOCLAW_CHAN_CRON       "cron"
#define OTTOCLAW_CHAN_HEARTBEAT  "heartbeat"

/* Message types on the bus */
typedef struct {
    char channel[16];       /* "dingtalk", "websocket", "cli" */
    char chat_id[32];       /* DingTalk user_id or WS client id */
    char *content;          /* Heap-allocated message text (caller must free) */
    char *media_path;       /* Heap-allocated media file path (caller must free) */
    char *reply_to;        /* Heap-allocated reply message ID (caller must free) */
    char *metadata;         /* Heap-allocated metadata JSON string (caller must free) */
} ottoclaw_msg_t;

/**
 * Initialize the message bus (inbound + outbound FreeRTOS queues).
 */
esp_err_t message_bus_init(void);

/**
 * Push a message to the inbound queue (towards Agent Loop).
 * The bus takes ownership of msg->content.
 */
esp_err_t message_bus_push_inbound(const ottoclaw_msg_t *msg);

/**
 * Pop a message from the inbound queue (blocking).
 * Caller must free msg->content when done.
 */
esp_err_t message_bus_pop_inbound(ottoclaw_msg_t *msg, uint32_t timeout_ms);

/**
 * Push a message to the outbound queue (towards channels).
 * The bus takes ownership of msg->content.
 */
esp_err_t message_bus_push_outbound(const ottoclaw_msg_t *msg);

/**
 * Pop a message from the outbound queue (blocking).
 * Caller must free msg->content when done.
 */
esp_err_t message_bus_pop_outbound(ottoclaw_msg_t *msg, uint32_t timeout_ms);

/**
 * Free all heap-allocated fields in a message.
 * Call this when done processing a message.
 */
void message_bus_free_msg(ottoclaw_msg_t *msg);
