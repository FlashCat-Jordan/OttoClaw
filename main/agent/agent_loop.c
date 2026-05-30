#include "agent_loop.h"
#include "agent/context_builder.h"
#include "ottoclaw_config.h"
#include "bus/message_bus.h"
#include "llm/llm_proxy.h"
#include "memory/session_mgr.h"
#include "tools/tool_registry.h"
#include "lcd/lcd_display.h"
#include "relation/relation.h"
#include "esp_task_wdt.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "cJSON.h"

static const char *TAG = "agent";

#define TOOL_OUTPUT_SIZE  (8 * 1024)

static bool text_contains_any(const char *text, const char *const *keywords, size_t count)
{
    if (!text || !text[0]) return false;
    for (size_t i = 0; i < count; i++) {
        if (keywords[i] && strstr(text, keywords[i])) return true;
    }
    return false;
}

static lcd_state_t choose_base_mood(const char *user_text, const char *assistant_text)
{
    static const char *const angry_words[] = {"生气", "烦", "讨厌", "怒", "滚", "闭嘴", "错了", "问题"};
    static const char *const shy_words[] = {"爱你", "喜欢", "抱抱", "亲", "么么", "害羞", "可爱"};
    static const char *const surprised_words[] = {"?", "？", "怎么", "居然", "竟然", "震惊", "惊讶"};
    static const char *const eating_words[] = {"吃", "饭", "饿", "面", "奶茶", "咖啡", "火锅"};
    static const char *const study_words[] = {"学习", "看书", "读", "作业", "题", "编程", "代码", "算法"};
    static const char *const sleep_words[] = {"睡", "晚安", "困", "休息"};
    static const char *const bored_words[] = {"无聊", "没意思", "发呆", "懒得"};
    static const char *const excited_words[] = {"!", "！", "太好了", "好耶", "厉害", "开心", "棒", "赞"};

    if (text_contains_any(user_text, angry_words, sizeof(angry_words) / sizeof(angry_words[0])) ||
        text_contains_any(assistant_text, angry_words, sizeof(angry_words) / sizeof(angry_words[0]))) {
        return LCD_STATE_ANGRY;
    }
    if (text_contains_any(user_text, shy_words, sizeof(shy_words) / sizeof(shy_words[0])) ||
        text_contains_any(assistant_text, shy_words, sizeof(shy_words) / sizeof(shy_words[0]))) {
        return LCD_STATE_SHY;
    }
    if (text_contains_any(user_text, eating_words, sizeof(eating_words) / sizeof(eating_words[0])) ||
        text_contains_any(assistant_text, eating_words, sizeof(eating_words) / sizeof(eating_words[0]))) {
        return LCD_STATE_EATING;
    }
    if (text_contains_any(user_text, study_words, sizeof(study_words) / sizeof(study_words[0])) ||
        text_contains_any(assistant_text, study_words, sizeof(study_words) / sizeof(study_words[0]))) {
        return LCD_STATE_STUDYING;
    }
    if (text_contains_any(user_text, sleep_words, sizeof(sleep_words) / sizeof(sleep_words[0])) ||
        text_contains_any(assistant_text, sleep_words, sizeof(sleep_words) / sizeof(sleep_words[0]))) {
        return LCD_STATE_SLEEPING;
    }
    if (text_contains_any(user_text, bored_words, sizeof(bored_words) / sizeof(bored_words[0])) ||
        text_contains_any(assistant_text, bored_words, sizeof(bored_words) / sizeof(bored_words[0]))) {
        return LCD_STATE_BORED;
    }
    if (text_contains_any(user_text, surprised_words, sizeof(surprised_words) / sizeof(surprised_words[0])) ||
        text_contains_any(assistant_text, surprised_words, sizeof(surprised_words) / sizeof(surprised_words[0]))) {
        return LCD_STATE_SURPRISED;
    }
    if (text_contains_any(user_text, excited_words, sizeof(excited_words) / sizeof(excited_words[0])) ||
        text_contains_any(assistant_text, excited_words, sizeof(excited_words) / sizeof(excited_words[0]))) {
        return LCD_STATE_EXCITED;
    }
    return LCD_STATE_HAPPY;
}

static bool should_trigger_motion(lcd_state_t mood)
{
    return mood == LCD_STATE_HAPPY ||
           mood == LCD_STATE_EXCITED ||
           mood == LCD_STATE_SHY ||
           mood == LCD_STATE_SURPRISED;
}

static void trigger_mood_motion(lcd_state_t mood)
{
    const char *tool_input = NULL;

    switch (mood) {
    case LCD_STATE_HAPPY:
        tool_input = "{\"action\":\"greeting\",\"steps\":1,\"direction\":1}";
        break;
    case LCD_STATE_EXCITED:
        tool_input = "{\"action\":\"swing\",\"steps\":1,\"speed\":700,\"amount\":18}";
        break;
    case LCD_STATE_SHY:
        tool_input = "{\"action\":\"shy\",\"steps\":1,\"direction\":1}";
        break;
    case LCD_STATE_SURPRISED:
        tool_input = "{\"action\":\"hands_up\",\"speed\":900,\"direction\":0}";
        break;
    default:
        break;
    }

    if (tool_input) {
        char motion_output[256] = {0};
        tool_registry_execute("self.otto.action", tool_input, motion_output, sizeof(motion_output));
        ESP_LOGI(TAG, "Mood motion result: %s", motion_output);
    }
}

/* Build the assistant content array from llm_response_t for the messages history.
 * Returns a cJSON array with text and tool_use blocks. */
static cJSON *build_assistant_content(const llm_response_t *resp)
{
    cJSON *content = cJSON_CreateArray();

    /* Text block */
    if (resp->text && resp->text_len > 0) {
        cJSON *text_block = cJSON_CreateObject();
        cJSON_AddStringToObject(text_block, "type", "text");
        cJSON_AddStringToObject(text_block, "text", resp->text);
        cJSON_AddItemToArray(content, text_block);
    }

    /* Tool use blocks */
    for (int i = 0; i < resp->call_count; i++) {
        const llm_tool_call_t *call = &resp->calls[i];
        cJSON *tool_block = cJSON_CreateObject();
        cJSON_AddStringToObject(tool_block, "type", "tool_use");
        cJSON_AddStringToObject(tool_block, "id", call->id);
        cJSON_AddStringToObject(tool_block, "name", call->name);

        cJSON *input = cJSON_Parse(call->input);
        if (input) {
            cJSON_AddItemToObject(tool_block, "input", input);
        } else {
            cJSON_AddItemToObject(tool_block, "input", cJSON_CreateObject());
        }

        cJSON_AddItemToArray(content, tool_block);
    }

    return content;
}

/* Build the user message with tool_result blocks */
static cJSON *build_tool_results(const llm_response_t *resp, char *tool_output, size_t tool_output_size)
{
    cJSON *content = cJSON_CreateArray();

    for (int i = 0; i < resp->call_count; i++) {
        const llm_tool_call_t *call = &resp->calls[i];

        /* Execute tool */
        tool_output[0] = '\0';
        tool_registry_execute(call->name, call->input, tool_output, tool_output_size);

        ESP_LOGI(TAG, "Tool %s result: %d bytes", call->name, (int)strlen(tool_output));

        /* Build tool_result block */
        cJSON *result_block = cJSON_CreateObject();
        cJSON_AddStringToObject(result_block, "type", "tool_result");
        cJSON_AddStringToObject(result_block, "tool_use_id", call->id);
        cJSON_AddStringToObject(result_block, "content", tool_output);
        cJSON_AddItemToArray(content, result_block);
    }

    return content;
}

static void agent_loop_task(void *arg)
{
    ESP_LOGI(TAG, "Agent loop started on core %d", xPortGetCoreID());

    /* Unsubscribe this task from the task watchdog — the agent loop
     * makes long-running HTTP calls (LLM + web search) that can take
     * 30-120s and would otherwise trigger spurious WDT resets */
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());
    esp_task_wdt_delete(NULL);  /* also remove current task handle alias */

    /* Allocate large buffers from PSRAM */
    char *system_prompt = heap_caps_calloc(1, OTTOCLAW_CONTEXT_BUF_SIZE, MALLOC_CAP_SPIRAM);
    char *history_json = heap_caps_calloc(1, OTTOCLAW_LLM_STREAM_BUF_SIZE, MALLOC_CAP_SPIRAM);
    char *tool_output = heap_caps_calloc(1, TOOL_OUTPUT_SIZE, MALLOC_CAP_SPIRAM);

    if (!system_prompt || !history_json || !tool_output) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffers");
        vTaskDelete(NULL);
        return;
    }

    const char *tools_json = tool_registry_get_tools_json();

    while (1) {
        ottoclaw_msg_t msg;
        esp_err_t err = message_bus_pop_inbound(&msg, UINT32_MAX);
        if (err != ESP_OK) continue;

        ESP_LOGI(TAG, "Processing message from %s:%s", msg.channel, msg.chat_id);

        /* 0. Increment relation counter (+1 per message, AI can subtract via update_relation) */
        relation_increment();

        /* 1. Build system prompt */
        context_build_system_prompt(system_prompt, OTTOCLAW_CONTEXT_BUF_SIZE);

        /* 2. Load session history into cJSON array */
        session_get_history_json(msg.chat_id, history_json,
                                 OTTOCLAW_LLM_STREAM_BUF_SIZE, OTTOCLAW_AGENT_MAX_HISTORY);

        cJSON *messages = cJSON_Parse(history_json);
        if (!messages) messages = cJSON_CreateArray();

        /* 3. Append current user message */
        cJSON *user_msg = cJSON_CreateObject();
        cJSON_AddStringToObject(user_msg, "role", "user");
        cJSON_AddStringToObject(user_msg, "content", msg.content);
        cJSON_AddItemToArray(messages, user_msg);

        /* Save user message NOW before LLM call — ensures context survives
         * even if LLM fails, times out, or WDT fires mid-call */
        session_append(msg.chat_id, "user", msg.content);

        lcd_set_state(LCD_STATE_THINKING);
        lcd_show_chat_message("user", msg.content);

        /* 4. ReAct loop */
        char *final_text = NULL;
        lcd_state_t chat_mood = lcd_get_base_mood();
        bool motion_triggered = false;
        int iteration = 0;

        while (iteration < OTTOCLAW_AGENT_MAX_TOOL_ITER) {
            llm_response_t resp;
            err = llm_chat_tools(system_prompt, messages, tools_json, &resp);

            if (err != ESP_OK) {
                ESP_LOGE(TAG, "LLM call failed: %s", esp_err_to_name(err));
                break;
            }

            if (!resp.tool_use) {
                /* Normal completion — save final text and break */
                if (resp.text && resp.text_len > 0) {
                    final_text = strdup(resp.text);
                }
                llm_response_free(&resp);
                break;
            }

            ESP_LOGI(TAG, "Tool use iteration %d: %d calls", iteration + 1, resp.call_count);

            /* Append assistant message with content array */
            cJSON *asst_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(asst_msg, "role", "assistant");
            cJSON_AddItemToObject(asst_msg, "content", build_assistant_content(&resp));
            cJSON_AddItemToArray(messages, asst_msg);

            /* Execute tools and append results */
            cJSON *tool_results = build_tool_results(&resp, tool_output, TOOL_OUTPUT_SIZE);
            cJSON *result_msg = cJSON_CreateObject();
            cJSON_AddStringToObject(result_msg, "role", "user");
            cJSON_AddItemToObject(result_msg, "content", tool_results);
            cJSON_AddItemToArray(messages, result_msg);

            llm_response_free(&resp);
            iteration++;
        }

        cJSON_Delete(messages);

        /* 5. Send response */
        if (final_text && final_text[0]) {
            chat_mood = choose_base_mood(msg.content, final_text);
            lcd_set_base_mood(chat_mood);

            /* Save assistant reply to session */
            session_append(msg.chat_id, "assistant", final_text);

            lcd_set_state(LCD_STATE_SPEAKING);

            /* Typewriter effect: stream character by character */
            lcd_stream_begin(true);  /* true = assistant side */
            const char *p = final_text;
            while (*p) {
                /* Determine UTF-8 char byte length */
                int char_len = 1;
                unsigned char c = (unsigned char)*p;
                if      (c >= 0xF0) char_len = 4;
                else if (c >= 0xE0) char_len = 3;
                else if (c >= 0xC0) char_len = 2;

                /* Feed one character (as chunk) */
                char chunk[5] = {0};
                for (int i = 0; i < char_len && p[i]; i++) chunk[i] = p[i];
                lcd_stream_append(chunk);
                p += char_len;

                /* 40ms per Chinese char, 20ms per ASCII */
                vTaskDelay(pdMS_TO_TICKS(char_len > 1 ? 40 : 20));
            }
            lcd_stream_end();

            if (!motion_triggered && should_trigger_motion(chat_mood)) {
                trigger_mood_motion(chat_mood);
                motion_triggered = true;
            }

            /* Push response to outbound */
            ottoclaw_msg_t out = {0};
            strncpy(out.channel, msg.channel, sizeof(out.channel) - 1);
            strncpy(out.chat_id, msg.chat_id, sizeof(out.chat_id) - 1);
            out.content = final_text;  /* transfer ownership */
            message_bus_push_outbound(&out);
        } else {
            /* Error or empty response */
            free(final_text);
            lcd_set_state(LCD_STATE_ERROR);
            lcd_show_chat_message("system", "Error: no response");

            ottoclaw_msg_t out = {0};
            strncpy(out.channel, msg.channel, sizeof(out.channel) - 1);
            strncpy(out.chat_id, msg.chat_id, sizeof(out.chat_id) - 1);
            out.content = strdup("Sorry, I encountered an error.");
            if (out.content) {
                message_bus_push_outbound(&out);
            }
        }

        /* Free inbound message content */
        free(msg.content);

        vTaskDelay(pdMS_TO_TICKS(3000));
        lcd_restore_base_mood();

        /* Log memory status */
        ESP_LOGI(TAG, "Free PSRAM: %d bytes",
                 (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
}

esp_err_t agent_loop_init(void)
{
    ESP_LOGI(TAG, "Agent loop initialized");
    return ESP_OK;
}

esp_err_t agent_loop_start(void)
{
    BaseType_t ret = xTaskCreatePinnedToCore(
        agent_loop_task, "agent_loop",
        OTTOCLAW_AGENT_STACK, NULL,
        OTTOCLAW_AGENT_PRIO, NULL, OTTOCLAW_AGENT_CORE);

    return (ret == pdPASS) ? ESP_OK : ESP_FAIL;
}
