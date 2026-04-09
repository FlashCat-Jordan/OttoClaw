#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    // 系统核心状态
    LCD_STATE_SLEEPING,       // 睡觉中
    LCD_STATE_CONNECTING,     // 联网中
    LCD_STATE_CONNECTED,      // 已连接
    LCD_STATE_ERROR,          // 网络挂了
    LCD_STATE_THINKING,       // 思考中
    LCD_STATE_SPEAKING,       // 在码字
    LCD_STATE_LISTENING,      // 在听

    // 人格情绪状态（可随机触发）
    LCD_STATE_HAPPY,          // 开心中
    LCD_STATE_DAYDREAM,       // 神游中
    LCD_STATE_IN_LOVE,        // 贪恋爱
    LCD_STATE_EATING,         // 吃饭中
    LCD_STATE_WORKOUT,        // 健身中
    LCD_STATE_STUDYING,       // 学习中
    LCD_STATE_WATCHING_TV,    // 看电视中
    LCD_STATE_IGNORING,       // 不想理你
    LCD_STATE_ANGRY,          // 生气中
    LCD_STATE_SURPRISED,      // 惊讶中
    LCD_STATE_BORED,          // 无聊中
    LCD_STATE_CYBER,          // 赛博模式
    LCD_STATE_DIZZY,          // 发晕中
    LCD_STATE_SHY,            // 害羞中
    LCD_STATE_EXCITED,        // 亢奋中

    // 配置模式
    LCD_STATE_CONFIG,         // 配置中（显示配网指引）

    LCD_STATE_COUNT
} lcd_state_t;

esp_err_t lcd_display_init(void);
void lcd_backlight_set(bool on);
void lcd_set_state(lcd_state_t state);
void lcd_show_chat_message(const char *role, const char *content);
void lcd_clear_chat(void);
void lcd_set_status_text(const char *text);

// 流式逐字显示 API
void lcd_stream_begin(bool is_assistant);          // 开始新一条流式消息
void lcd_stream_append(const char *chunk);         // 追加文本块（可多字符）
void lcd_stream_end(void);                         // 结束当前流式消息

// 配网提示覆盖层（配置模式）
void lcd_show_qr_overlay(const char *url, const char *hint);
void lcd_hide_qr_overlay(void);
