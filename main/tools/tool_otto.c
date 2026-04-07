#include "tool_registry.h"
#include "otto/otto_movements.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "tool_otto";

extern otto_t g_otto;

static esp_err_t tool_otto_action_execute(const char *input_json, char *output, size_t output_size) {
    cJSON *input = cJSON_Parse(input_json);
    if (!input) {
        snprintf(output, output_size, "Error: Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *action = cJSON_GetObjectItem(input, "action");
    if (!cJSON_IsString(action)) {
        cJSON_Delete(input);
        snprintf(output, output_size, "Error: Missing 'action' field");
        return ESP_FAIL;
    }

    const char *action_name = action->valuestring;
    int steps = 3;
    int speed = 1000;
    int direction = 1;
    int amount = 25;

    cJSON *steps_item = cJSON_GetObjectItem(input, "steps");
    if (cJSON_IsNumber(steps_item)) {
        steps = steps_item->valueint;
        if (steps < 1) steps = 1;
        if (steps > 100) steps = 100;
    }

    cJSON *speed_item = cJSON_GetObjectItem(input, "speed");
    if (cJSON_IsNumber(speed_item)) {
        speed = speed_item->valueint;
        if (speed < 100) speed = 100;
        if (speed > 3000) speed = 3000;
    }

    cJSON *direction_item = cJSON_GetObjectItem(input, "direction");
    if (cJSON_IsNumber(direction_item)) {
        direction = direction_item->valueint;
        if (direction < -1) direction = -1;
        if (direction > 1) direction = 1;
    }

    cJSON *amount_item = cJSON_GetObjectItem(input, "amount");
    if (cJSON_IsNumber(amount_item)) {
        amount = amount_item->valueint;
        if (amount < 0) amount = 0;
        if (amount > 170) amount = 170;
    }

    ESP_LOGI(TAG, "Executing action: %s, steps=%d, speed=%d, direction=%d, amount=%d",
             action_name, steps, speed, direction, amount);

    if (strcmp(action_name, "home") == 0) {
        otto_home(&g_otto, true);
        snprintf(output, output_size, "已回到初始位置");
    } else if (strcmp(action_name, "walk") == 0) {
        otto_walk(&g_otto, steps, speed, direction, amount);
        snprintf(output, output_size, "我向前走了 %d 步", steps);
    } else if (strcmp(action_name, "walk_backward") == 0) {
        otto_walk(&g_otto, steps, speed, BACKWARD, amount);
        snprintf(output, output_size, "我向后退了 %d 步", steps);
    } else if (strcmp(action_name, "turn") == 0) {
        otto_turn(&g_otto, steps, speed, direction, amount);
        snprintf(output, output_size, "我向%s转了 %d 度", direction == LEFT ? "左" : "右", steps * 90);
    } else if (strcmp(action_name, "jump") == 0) {
        otto_jump(&g_otto, steps, speed);
        snprintf(output, output_size, "我跳了 %d 下", steps);
    } else if (strcmp(action_name, "swing") == 0) {
        otto_swing(&g_otto, steps, speed, amount);
        snprintf(output, output_size, "我摇摆了 %d 下", steps);
    } else if (strcmp(action_name, "moonwalk") == 0) {
        otto_moonwalker(&g_otto, steps, speed, amount, direction);
        snprintf(output, output_size, "我跳了太空步");
    } else if (strcmp(action_name, "bend") == 0) {
        otto_bend(&g_otto, steps, speed, direction);
        snprintf(output, output_size, "我弯了一下身体");
    } else if (strcmp(action_name, "shake_leg") == 0) {
        otto_shake_leg(&g_otto, steps, speed, direction);
        snprintf(output, output_size, "我摇了摇腿");
    } else if (strcmp(action_name, "sit") == 0) {
        otto_sit(&g_otto);
        snprintf(output, output_size, "我坐下了");
    } else if (strcmp(action_name, "updown") == 0) {
        otto_updown(&g_otto, steps, speed, amount);
        snprintf(output, output_size, "我上下运动了 %d 下", steps);
    } else if (strcmp(action_name, "hands_up") == 0) {
        otto_hands_up(&g_otto, speed, direction);
        snprintf(output, output_size, "我举起了手");
    } else if (strcmp(action_name, "hands_down") == 0) {
        otto_hands_down(&g_otto, speed, direction);
        snprintf(output, output_size, "我放下了手");
    } else if (strcmp(action_name, "hand_wave") == 0) {
        otto_hand_wave(&g_otto, direction);
        snprintf(output, output_size, "我挥了挥手");
    } else if (strcmp(action_name, "windmill") == 0) {
        otto_windmill(&g_otto, steps, speed, amount);
        snprintf(output, output_size, "我转了一个大风车");
    } else if (strcmp(action_name, "takeoff") == 0) {
        otto_takeoff(&g_otto, steps, speed, amount);
        snprintf(output, output_size, "我要起飞啦！");
    } else if (strcmp(action_name, "fitness") == 0) {
        otto_fitness(&g_otto, steps, speed, amount);
        snprintf(output, output_size, "我做健身操！");
    } else if (strcmp(action_name, "greeting") == 0) {
        otto_greeting(&g_otto, direction, steps);
        snprintf(output, output_size, "你好！");
    } else if (strcmp(action_name, "shy") == 0) {
        otto_shy(&g_otto, direction, steps);
        snprintf(output, output_size, "哎呀，好害羞~");
    } else if (strcmp(action_name, "radio_calisthenics") == 0) {
        otto_radio_calisthenics(&g_otto);
        snprintf(output, output_size, "广播体操开始！");
    } else if (strcmp(action_name, "magic_circle") == 0) {
        otto_magic_circle(&g_otto);
        snprintf(output, output_size, "爱的魔力转圈圈~");
    } else if (strcmp(action_name, "showcase") == 0) {
        otto_showcase(&g_otto);
        snprintf(output, output_size, "这是我的表演！");
    } else {
        snprintf(output, output_size, "未知动作: %s", action_name);
        cJSON_Delete(input);
        return ESP_FAIL;
    }

    cJSON_Delete(input);
    return ESP_OK;
}

static const char *otto_tool_schema =
    "{\"type\":\"object\","
    "\"properties\":{"
        "\"action\":{\"type\":\"string\",\"description\":\"动作名称: walk/turn/jump/swing/moonwalk/bend/shake_leg/sit/updown/hands_up/hands_down/hand_wave/windmill/takeoff/fitness/greeting/shy/radio_calisthenics/magic_circle/showcase/home\"},"
        "\"steps\":{\"type\":\"number\",\"description\":\"动作次数1-100，默认3\"},"
        "\"speed\":{\"type\":\"number\",\"description\":\"速度100-3000，越小越快，默认1000\"},"
        "\"direction\":{\"type\":\"number\",\"description\":\"方向: 1=前进/左转，-1=后退/右转，0=双手\"},"
        "\"amount\":{\"type\":\"number\",\"description\":\"幅度0-170，默认25\"}"
    "},"
    "\"required\":[\"action\"]"
    "}";

void tool_otto_register(void) {
    ESP_LOGI(TAG, "Registering otto tools...");

    mimi_tool_t tool = {
        .name = "self.otto.action",
        .description = "控制机器人执行动作",
        .input_schema_json = otto_tool_schema,
        .execute = tool_otto_action_execute
    };

    tool_registry_register(&tool);
    ESP_LOGI(TAG, "Otto tool registered successfully");
}