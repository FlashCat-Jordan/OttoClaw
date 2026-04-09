/*
 * lcd_display.c — MimiClaw 像素风机甲座舱 UI
 * 全新设计：小人坐在机甲座舱中，22种状态表情，流式逐字显示
 */
#include "lcd_display.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"

LV_FONT_DECLARE(font_chinese_14);

static const char *TAG = "lcd";

// ═══════════════════════════════════════════════════════════
//  硬件配置
// ═══════════════════════════════════════════════════════════
#define LCD_HOST        SPI3_HOST
#define LCD_PIXEL_CLK   (40 * 1000 * 1000)
#define PIN_SCLK        9
#define PIN_MOSI        10
#define PIN_DC          46
#define PIN_RST         11
#define PIN_CS          12
#define PIN_BL          3
#define LCD_H_RES       240
#define LCD_V_RES       240
#define QR_CANVAS_PX    152   /* QR码画布边长（像素） */

// ═══════════════════════════════════════════════════════════
//  颜色定义
// ═══════════════════════════════════════════════════════════
#define C_BG            lv_color_hex(0x080818)  // 屏幕背景
#define C_FACE_BG       lv_color_hex(0x0A0A20)  // 脸部背景
#define C_MOUTH_BG      lv_color_hex(0x0C0C22)  // 对话区背景
#define C_MOUTH_BORDER  lv_color_hex(0x1A1A4A)
#define C_TEXT_USER     lv_color_hex(0x80FFB0)
#define C_TEXT_ASSIST   lv_color_hex(0xB0B0FF)
#define C_TEXT_DIM      lv_color_hex(0x505070)

// 机甲座舱颜色
#define C_MECHA_DARK    lv_color_hex(0x0D1520)  // 机甲主体深色
#define C_MECHA_MID     lv_color_hex(0x1A2A3A)  // 机甲中间色
#define C_MECHA_EDGE    lv_color_hex(0x2A3A4A)  // 机甲边缘
#define C_COCKPIT_GLASS lv_color_hex(0x0A1828)  // 座舱玻璃深蓝
#define C_PILOT_SKIN    lv_color_hex(0xF4C48A)  // 小人肤色
#define C_PILOT_SUIT    lv_color_hex(0x1A3A6A)  // 驾驶服

// 状态主题色（座舱发光颜色）
#define C_CYAN          lv_color_hex(0x00E5FF)
#define C_PURPLE        lv_color_hex(0x8B2FF7)
#define C_GREEN         lv_color_hex(0x00FF88)
#define C_ORANGE        lv_color_hex(0xFFAA00)
#define C_RED           lv_color_hex(0xFF3030)
#define C_PINK          lv_color_hex(0xFF69B4)
#define C_YELLOW        lv_color_hex(0xFFE000)
#define C_LIME          lv_color_hex(0x39FF14)
#define C_GOLD          lv_color_hex(0xFFD700)
#define C_BLUE          lv_color_hex(0x4488FF)
#define C_GRAY          lv_color_hex(0x607080)
#define C_WHITE         lv_color_hex(0xE0EEFF)
#define C_DIM_BLUE      lv_color_hex(0x1A2A4A)

// ═══════════════════════════════════════════════════════════
//  硬件句柄
// ═══════════════════════════════════════════════════════════
static esp_lcd_panel_io_handle_t lcd_io    = NULL;
static esp_lcd_panel_handle_t    lcd_panel = NULL;
static lv_display_t             *lvgl_disp = NULL;

// ═══════════════════════════════════════════════════════════
//  UI 对象声明
// ═══════════════════════════════════════════════════════════
static lv_obj_t *screen       = NULL;
static lv_obj_t *face_area    = NULL;   // 上半区 240×128

// ── 机甲主体 ──
static lv_obj_t *mecha_body   = NULL;   // 机甲身体（大矩形）
static lv_obj_t *mecha_arm_l  = NULL;   // 左手臂
static lv_obj_t *mecha_arm_r  = NULL;   // 右手臂
static lv_obj_t *thruster_l   = NULL;   // 左推进器
static lv_obj_t *thruster_r   = NULL;   // 右推进器
static lv_obj_t *mecha_leg_l  = NULL;   // 左腿
static lv_obj_t *mecha_leg_r  = NULL;   // 右腿

// ── 座舱 ──
static lv_obj_t *cockpit_outer = NULL;  // 座舱外框
static lv_obj_t *cockpit_glass = NULL;  // 座舱玻璃（带边框发光）
static lv_obj_t *scan_line     = NULL;  // 扫描线（联网动画）

// ── 小人头部（在座舱内） ──
static lv_obj_t *pilot_head   = NULL;
static lv_obj_t *pilot_eye_l  = NULL;
static lv_obj_t *pilot_eye_r  = NULL;
static lv_obj_t *pilot_brow_l = NULL;  // 眉毛
static lv_obj_t *pilot_brow_r = NULL;
static lv_obj_t *pilot_mouth  = NULL;
static lv_obj_t *pilot_blush_l = NULL; // 腮红（害羞）
static lv_obj_t *pilot_blush_r = NULL;
static lv_obj_t *sweat_drop   = NULL;  // 汗珠

// ── 装饰特效 ──
static lv_obj_t *heart_l      = NULL;  // 心形（恋爱）
static lv_obj_t *heart_r      = NULL;
static lv_obj_t *star_l       = NULL;  // 星星
static lv_obj_t *star_r       = NULL;
static lv_obj_t *zzz_label    = NULL;  // ZZZ（睡觉）
static lv_obj_t *exclaim      = NULL;  // ！（惊讶）

// ── 对话区 ──
static lv_obj_t *mouth_area   = NULL;
static lv_obj_t *stream_label = NULL;  // 当前流式消息label
static lv_obj_t *wifi_icon    = NULL;
static lv_obj_t *status_label = NULL;

// ── QR 覆盖层 ──
static lv_obj_t *qr_overlay   = NULL;  // 全屏遮罩
static lv_obj_t *qr_canvas    = NULL;  // QR码画布

// ── 状态 ──
static lcd_state_t current_state = LCD_STATE_SLEEPING;
static const lv_font_t *font_cn  = NULL;
static const lv_font_t *font_sm  = NULL;

// ── 流式缓冲 ──
#define STREAM_BUF_SIZE 1024
static char     stream_buf[STREAM_BUF_SIZE];
static int      stream_len  = 0;
static bool     stream_active = false;
static lv_color_t stream_color;

// ── 随机情绪 ──
static uint32_t mood_ticks = 0;

// ═══════════════════════════════════════════════════════════
//  状态文字映射
// ═══════════════════════════════════════════════════════════
static const char *state_labels[LCD_STATE_COUNT] = {
    [LCD_STATE_SLEEPING]     = "在睡觉",
    [LCD_STATE_CONNECTING]   = "联网中",
    [LCD_STATE_CONNECTED]    = "已就绪",
    [LCD_STATE_ERROR]        = "网络挂了",
    [LCD_STATE_THINKING]     = "思考中",
    [LCD_STATE_SPEAKING]     = "在码字",
    [LCD_STATE_LISTENING]    = "在听",
    [LCD_STATE_HAPPY]        = "开心中",
    [LCD_STATE_DAYDREAM]     = "神游中",
    [LCD_STATE_IN_LOVE]      = "贪恋爱",
    [LCD_STATE_EATING]       = "吃饭中",
    [LCD_STATE_WORKOUT]      = "健身中",
    [LCD_STATE_STUDYING]     = "学习中",
    [LCD_STATE_WATCHING_TV]  = "看电视中",
    [LCD_STATE_IGNORING]     = "不想理你",
    [LCD_STATE_ANGRY]        = "生气中",
    [LCD_STATE_SURPRISED]    = "惊讶中",
    [LCD_STATE_BORED]        = "无聊中",
    [LCD_STATE_CYBER]        = "赛博模式",
    [LCD_STATE_DIZZY]        = "发晕中",
    [LCD_STATE_SHY]          = "害羞中",
    [LCD_STATE_EXCITED]      = "亢奋中",
    [LCD_STATE_CONFIG]       = "配置中",
};

// ═══════════════════════════════════════════════════════════
//  工具函数
// ═══════════════════════════════════════════════════════════

void lcd_backlight_set(bool on)
{
    gpio_set_level(PIN_BL, on ? 1 : 0);
}

static void stop_all_anims(void)
{
    lv_obj_t *objs[] = {
        mecha_body, mecha_arm_l, mecha_arm_r, thruster_l, thruster_r,
        mecha_leg_l, mecha_leg_r,
        cockpit_outer, cockpit_glass, scan_line,
        pilot_head, pilot_eye_l, pilot_eye_r,
        pilot_brow_l, pilot_brow_r, pilot_mouth,
        pilot_blush_l, pilot_blush_r, sweat_drop,
        heart_l, heart_r, star_l, star_r, zzz_label, exclaim,
        face_area
    };
    for (int i = 0; i < (int)(sizeof(objs)/sizeof(objs[0])); i++) {
        if (objs[i]) lv_anim_delete(objs[i], NULL);
    }
}

// 隐藏所有情绪装饰
static void hide_all_decorations(void)
{
    lv_obj_t *deco[] = {
        pilot_blush_l, pilot_blush_r, sweat_drop,
        heart_l, heart_r, star_l, star_r, zzz_label, exclaim, scan_line
    };
    for (int i = 0; i < (int)(sizeof(deco)/sizeof(deco[0])); i++) {
        if (deco[i]) lv_obj_add_flag(deco[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// 动画回调
static void anim_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}
static void anim_border_opa_cb(void *var, int32_t v)
{
    lv_obj_set_style_border_opa((lv_obj_t*)var, (lv_opa_t)v, 0);
}
static void anim_x_cb(void *var, int32_t v)   { lv_obj_set_x((lv_obj_t*)var, v); }
static void anim_y_cb(void *var, int32_t v)   { lv_obj_set_y((lv_obj_t*)var, v); }
static void anim_w_cb(void *var, int32_t v)   { lv_obj_set_width((lv_obj_t*)var, v); }
static void anim_h_cb(void *var, int32_t v)   { lv_obj_set_height((lv_obj_t*)var, v); }

static void make_inf(lv_obj_t *obj, lv_anim_exec_xcb_t cb,
                     int32_t from, int32_t to, uint32_t dur, uint32_t pb)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, dur);
    lv_anim_set_playback_time(&a, pb);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void make_once(lv_obj_t *obj, lv_anim_exec_xcb_t cb,
                      int32_t from, int32_t to, uint32_t dur, uint32_t pb)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, dur);
    lv_anim_set_playback_time(&a, pb);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_start(&a);
}

// 设置座舱发光颜色
static void set_cockpit_glow(lv_color_t col)
{
    if (cockpit_glass) {
        lv_obj_set_style_border_color(cockpit_glass, col, 0);
    }
    if (cockpit_outer) {
        lv_obj_set_style_border_color(cockpit_outer, col, 0);
    }
}

// 设置眼睛颜色
static void set_eye_color(lv_color_t col)
{
    if (pilot_eye_l) lv_obj_set_style_bg_color(pilot_eye_l, col, 0);
    if (pilot_eye_r) lv_obj_set_style_bg_color(pilot_eye_r, col, 0);
}

// 设置眉毛高度偏移（正=压低=皱眉，负=抬高）
static void set_brow_offset(int32_t offset_l, int32_t offset_r)
{
    if (pilot_brow_l) lv_obj_set_y(pilot_brow_l, -19 + offset_l);
    if (pilot_brow_r) lv_obj_set_y(pilot_brow_r, -19 + offset_r);
}

// 重置所有表情到默认
static void reset_face(void)
{
    // 眉毛默认位置
    set_brow_offset(0, 0);
    if (pilot_brow_l) { lv_obj_set_width(pilot_brow_l, 10); lv_obj_set_x(pilot_brow_l, -13); }
    if (pilot_brow_r) { lv_obj_set_width(pilot_brow_r, 10); lv_obj_set_x(pilot_brow_r, 3); }

    // 眼睛默认：圆形，默认大小 8×8
    if (pilot_eye_l) {
        lv_obj_set_size(pilot_eye_l, 8, 8);
        lv_obj_set_style_radius(pilot_eye_l, 4, 0);
        lv_obj_set_style_bg_opa(pilot_eye_l, LV_OPA_COVER, 0);
        lv_obj_align(pilot_eye_l, LV_ALIGN_CENTER, -9, -4);
    }
    if (pilot_eye_r) {
        lv_obj_set_size(pilot_eye_r, 8, 8);
        lv_obj_set_style_radius(pilot_eye_r, 4, 0);
        lv_obj_set_style_bg_opa(pilot_eye_r, LV_OPA_COVER, 0);
        lv_obj_align(pilot_eye_r, LV_ALIGN_CENTER, 9, -4);
    }
    set_eye_color(lv_color_hex(0x2A1A0A));

    // 嘴巴默认：小横线
    if (pilot_mouth) {
        lv_obj_set_size(pilot_mouth, 14, 3);
        lv_obj_set_style_radius(pilot_mouth, 1, 0);
        lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x3A1A0A), 0);
        lv_obj_set_style_bg_opa(pilot_mouth, LV_OPA_COVER, 0);
        lv_obj_align(pilot_mouth, LV_ALIGN_CENTER, 0, 8);
    }

    // 机甲手臂恢复
    if (mecha_arm_l) lv_obj_align(mecha_arm_l, LV_ALIGN_CENTER, -58, 20);
    if (mecha_arm_r) lv_obj_align(mecha_arm_r, LV_ALIGN_CENTER, 58, 20);

    // 座舱玻璃边框透明度
    if (cockpit_glass) lv_obj_set_style_border_opa(cockpit_glass, LV_OPA_80, 0);
    if (cockpit_outer) lv_obj_set_style_border_opa(cockpit_outer, LV_OPA_60, 0);
}

// ═══════════════════════════════════════════════════════════
//  UI 构建 — 机甲座舱
// ═══════════════════════════════════════════════════════════

static void build_mecha_ui(void)
{
    // ── 机甲腿部（最底层） ──────────────────────────────────
    mecha_leg_l = lv_obj_create(face_area);
    lv_obj_set_size(mecha_leg_l, 18, 28);
    lv_obj_set_style_radius(mecha_leg_l, 4, 0);
    lv_obj_set_style_bg_color(mecha_leg_l, C_MECHA_DARK, 0);
    lv_obj_set_style_bg_opa(mecha_leg_l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mecha_leg_l, 1, 0);
    lv_obj_set_style_border_color(mecha_leg_l, C_MECHA_EDGE, 0);
    lv_obj_set_style_border_opa(mecha_leg_l, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(mecha_leg_l, 0, 0);
    lv_obj_clear_flag(mecha_leg_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(mecha_leg_l, LV_ALIGN_BOTTOM_MID, -22, -2);

    mecha_leg_r = lv_obj_create(face_area);
    lv_obj_set_size(mecha_leg_r, 18, 28);
    lv_obj_set_style_radius(mecha_leg_r, 4, 0);
    lv_obj_set_style_bg_color(mecha_leg_r, C_MECHA_DARK, 0);
    lv_obj_set_style_bg_opa(mecha_leg_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mecha_leg_r, 1, 0);
    lv_obj_set_style_border_color(mecha_leg_r, C_MECHA_EDGE, 0);
    lv_obj_set_style_border_opa(mecha_leg_r, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(mecha_leg_r, 0, 0);
    lv_obj_clear_flag(mecha_leg_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(mecha_leg_r, LV_ALIGN_BOTTOM_MID, 22, -2);

    // ── 机甲身体 ─────────────────────────────────────────────
    mecha_body = lv_obj_create(face_area);
    lv_obj_set_size(mecha_body, 120, 50);
    lv_obj_set_style_radius(mecha_body, 8, 0);
    lv_obj_set_style_bg_color(mecha_body, C_MECHA_MID, 0);
    lv_obj_set_style_bg_opa(mecha_body, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mecha_body, 2, 0);
    lv_obj_set_style_border_color(mecha_body, C_MECHA_EDGE, 0);
    lv_obj_set_style_border_opa(mecha_body, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(mecha_body, 0, 0);
    lv_obj_clear_flag(mecha_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(mecha_body, LV_ALIGN_BOTTOM_MID, 0, -28);

    // 身体中线装饰
    lv_obj_t *body_line = lv_obj_create(mecha_body);
    lv_obj_set_size(body_line, 2, 36);
    lv_obj_set_style_bg_color(body_line, C_MECHA_EDGE, 0);
    lv_obj_set_style_bg_opa(body_line, LV_OPA_60, 0);
    lv_obj_set_style_border_width(body_line, 0, 0);
    lv_obj_set_style_pad_all(body_line, 0, 0);
    lv_obj_clear_flag(body_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(body_line, LV_ALIGN_CENTER, 0, 0);

    // ── 手臂 ──────────────────────────────────────────────
    mecha_arm_l = lv_obj_create(face_area);
    lv_obj_set_size(mecha_arm_l, 22, 40);
    lv_obj_set_style_radius(mecha_arm_l, 6, 0);
    lv_obj_set_style_bg_color(mecha_arm_l, C_MECHA_DARK, 0);
    lv_obj_set_style_bg_opa(mecha_arm_l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mecha_arm_l, 1, 0);
    lv_obj_set_style_border_color(mecha_arm_l, C_MECHA_EDGE, 0);
    lv_obj_set_style_border_opa(mecha_arm_l, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(mecha_arm_l, 0, 0);
    lv_obj_clear_flag(mecha_arm_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(mecha_arm_l, LV_ALIGN_CENTER, -58, 20);

    mecha_arm_r = lv_obj_create(face_area);
    lv_obj_set_size(mecha_arm_r, 22, 40);
    lv_obj_set_style_radius(mecha_arm_r, 6, 0);
    lv_obj_set_style_bg_color(mecha_arm_r, C_MECHA_DARK, 0);
    lv_obj_set_style_bg_opa(mecha_arm_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(mecha_arm_r, 1, 0);
    lv_obj_set_style_border_color(mecha_arm_r, C_MECHA_EDGE, 0);
    lv_obj_set_style_border_opa(mecha_arm_r, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(mecha_arm_r, 0, 0);
    lv_obj_clear_flag(mecha_arm_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(mecha_arm_r, LV_ALIGN_CENTER, 58, 20);

    // ── 推进器（手臂末端小圆） ────────────────────────────
    thruster_l = lv_obj_create(face_area);
    lv_obj_set_size(thruster_l, 14, 14);
    lv_obj_set_style_radius(thruster_l, 7, 0);
    lv_obj_set_style_bg_color(thruster_l, C_MECHA_DARK, 0);
    lv_obj_set_style_bg_opa(thruster_l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(thruster_l, 2, 0);
    lv_obj_set_style_border_color(thruster_l, C_CYAN, 0);
    lv_obj_set_style_border_opa(thruster_l, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(thruster_l, 0, 0);
    lv_obj_clear_flag(thruster_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(thruster_l, LV_ALIGN_CENTER, -58, 44);

    thruster_r = lv_obj_create(face_area);
    lv_obj_set_size(thruster_r, 14, 14);
    lv_obj_set_style_radius(thruster_r, 7, 0);
    lv_obj_set_style_bg_color(thruster_r, C_MECHA_DARK, 0);
    lv_obj_set_style_bg_opa(thruster_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(thruster_r, 2, 0);
    lv_obj_set_style_border_color(thruster_r, C_CYAN, 0);
    lv_obj_set_style_border_opa(thruster_r, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(thruster_r, 0, 0);
    lv_obj_clear_flag(thruster_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(thruster_r, LV_ALIGN_CENTER, 58, 44);

    // ── 座舱外框 ──────────────────────────────────────────
    cockpit_outer = lv_obj_create(face_area);
    lv_obj_set_size(cockpit_outer, 86, 90);
    lv_obj_set_style_radius(cockpit_outer, 18, 0);
    lv_obj_set_style_bg_color(cockpit_outer, C_MECHA_DARK, 0);
    lv_obj_set_style_bg_opa(cockpit_outer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cockpit_outer, 3, 0);
    lv_obj_set_style_border_color(cockpit_outer, C_CYAN, 0);
    lv_obj_set_style_border_opa(cockpit_outer, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(cockpit_outer, 0, 0);
    lv_obj_clear_flag(cockpit_outer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(cockpit_outer, LV_ALIGN_CENTER, 0, -18);

    // ── 座舱玻璃（内层，深蓝透明感） ─────────────────────
    cockpit_glass = lv_obj_create(cockpit_outer);
    lv_obj_set_size(cockpit_glass, 72, 76);
    lv_obj_set_style_radius(cockpit_glass, 14, 0);
    lv_obj_set_style_bg_color(cockpit_glass, C_COCKPIT_GLASS, 0);
    lv_obj_set_style_bg_opa(cockpit_glass, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cockpit_glass, 2, 0);
    lv_obj_set_style_border_color(cockpit_glass, C_CYAN, 0);
    lv_obj_set_style_border_opa(cockpit_glass, LV_OPA_80, 0);
    lv_obj_set_style_pad_all(cockpit_glass, 0, 0);
    lv_obj_clear_flag(cockpit_glass, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(cockpit_glass, LV_ALIGN_CENTER, 0, 0);

    // ── 扫描线（隐藏，联网时使用） ───────────────────────
    scan_line = lv_obj_create(cockpit_glass);
    lv_obj_set_size(scan_line, 72, 3);
    lv_obj_set_style_radius(scan_line, 0, 0);
    lv_obj_set_style_bg_color(scan_line, C_ORANGE, 0);
    lv_obj_set_style_bg_opa(scan_line, LV_OPA_60, 0);
    lv_obj_set_style_border_width(scan_line, 0, 0);
    lv_obj_set_style_pad_all(scan_line, 0, 0);
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(scan_line, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(scan_line, LV_OBJ_FLAG_HIDDEN);

    // ── 小人头部（在座舱玻璃内） ─────────────────────────
    pilot_head = lv_obj_create(cockpit_glass);
    lv_obj_set_size(pilot_head, 38, 42);
    lv_obj_set_style_radius(pilot_head, 10, 0);
    lv_obj_set_style_bg_color(pilot_head, C_PILOT_SKIN, 0);
    lv_obj_set_style_bg_opa(pilot_head, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pilot_head, 0, 0);
    lv_obj_set_style_pad_all(pilot_head, 0, 0);
    lv_obj_clear_flag(pilot_head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pilot_head, LV_ALIGN_CENTER, 0, -4);

    // ── 驾驶服领口 ─────────────────────────────────────
    lv_obj_t *suit = lv_obj_create(pilot_head);
    lv_obj_set_size(suit, 38, 14);
    lv_obj_set_style_radius(suit, 0, 0);
    lv_obj_set_style_bg_color(suit, C_PILOT_SUIT, 0);
    lv_obj_set_style_bg_opa(suit, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(suit, 0, 0);
    lv_obj_set_style_pad_all(suit, 0, 0);
    lv_obj_clear_flag(suit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(suit, LV_ALIGN_BOTTOM_MID, 0, 0);

    // ── 眉毛 ──────────────────────────────────────────────
    pilot_brow_l = lv_obj_create(pilot_head);
    lv_obj_set_size(pilot_brow_l, 10, 2);
    lv_obj_set_style_radius(pilot_brow_l, 1, 0);
    lv_obj_set_style_bg_color(pilot_brow_l, lv_color_hex(0x2A1000), 0);
    lv_obj_set_style_bg_opa(pilot_brow_l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pilot_brow_l, 0, 0);
    lv_obj_set_style_pad_all(pilot_brow_l, 0, 0);
    lv_obj_clear_flag(pilot_brow_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(pilot_brow_l, 5, 8);

    pilot_brow_r = lv_obj_create(pilot_head);
    lv_obj_set_size(pilot_brow_r, 10, 2);
    lv_obj_set_style_radius(pilot_brow_r, 1, 0);
    lv_obj_set_style_bg_color(pilot_brow_r, lv_color_hex(0x2A1000), 0);
    lv_obj_set_style_bg_opa(pilot_brow_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pilot_brow_r, 0, 0);
    lv_obj_set_style_pad_all(pilot_brow_r, 0, 0);
    lv_obj_clear_flag(pilot_brow_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(pilot_brow_r, 23, 8);

    // ── 眼睛 ──────────────────────────────────────────────
    pilot_eye_l = lv_obj_create(pilot_head);
    lv_obj_set_size(pilot_eye_l, 8, 8);
    lv_obj_set_style_radius(pilot_eye_l, 4, 0);
    lv_obj_set_style_bg_color(pilot_eye_l, lv_color_hex(0x2A1A0A), 0);
    lv_obj_set_style_bg_opa(pilot_eye_l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pilot_eye_l, 0, 0);
    lv_obj_set_style_pad_all(pilot_eye_l, 0, 0);
    lv_obj_clear_flag(pilot_eye_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pilot_eye_l, LV_ALIGN_CENTER, -9, -4);

    pilot_eye_r = lv_obj_create(pilot_head);
    lv_obj_set_size(pilot_eye_r, 8, 8);
    lv_obj_set_style_radius(pilot_eye_r, 4, 0);
    lv_obj_set_style_bg_color(pilot_eye_r, lv_color_hex(0x2A1A0A), 0);
    lv_obj_set_style_bg_opa(pilot_eye_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pilot_eye_r, 0, 0);
    lv_obj_set_style_pad_all(pilot_eye_r, 0, 0);
    lv_obj_clear_flag(pilot_eye_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pilot_eye_r, LV_ALIGN_CENTER, 9, -4);

    // ── 嘴巴 ──────────────────────────────────────────────
    pilot_mouth = lv_obj_create(pilot_head);
    lv_obj_set_size(pilot_mouth, 14, 3);
    lv_obj_set_style_radius(pilot_mouth, 1, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x3A1A0A), 0);
    lv_obj_set_style_bg_opa(pilot_mouth, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pilot_mouth, 0, 0);
    lv_obj_set_style_pad_all(pilot_mouth, 0, 0);
    lv_obj_clear_flag(pilot_mouth, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pilot_mouth, LV_ALIGN_CENTER, 0, 8);

    // ── 腮红（害羞时显示） ────────────────────────────────
    pilot_blush_l = lv_obj_create(pilot_head);
    lv_obj_set_size(pilot_blush_l, 10, 6);
    lv_obj_set_style_radius(pilot_blush_l, 3, 0);
    lv_obj_set_style_bg_color(pilot_blush_l, lv_color_hex(0xFF8080), 0);
    lv_obj_set_style_bg_opa(pilot_blush_l, LV_OPA_50, 0);
    lv_obj_set_style_border_width(pilot_blush_l, 0, 0);
    lv_obj_set_style_pad_all(pilot_blush_l, 0, 0);
    lv_obj_clear_flag(pilot_blush_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pilot_blush_l, LV_ALIGN_CENTER, -14, 2);
    lv_obj_add_flag(pilot_blush_l, LV_OBJ_FLAG_HIDDEN);

    pilot_blush_r = lv_obj_create(pilot_head);
    lv_obj_set_size(pilot_blush_r, 10, 6);
    lv_obj_set_style_radius(pilot_blush_r, 3, 0);
    lv_obj_set_style_bg_color(pilot_blush_r, lv_color_hex(0xFF8080), 0);
    lv_obj_set_style_bg_opa(pilot_blush_r, LV_OPA_50, 0);
    lv_obj_set_style_border_width(pilot_blush_r, 0, 0);
    lv_obj_set_style_pad_all(pilot_blush_r, 0, 0);
    lv_obj_clear_flag(pilot_blush_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pilot_blush_r, LV_ALIGN_CENTER, 14, 2);
    lv_obj_add_flag(pilot_blush_r, LV_OBJ_FLAG_HIDDEN);

    // ── 汗珠 ──────────────────────────────────────────────
    sweat_drop = lv_obj_create(cockpit_glass);
    lv_obj_set_size(sweat_drop, 6, 10);
    lv_obj_set_style_radius(sweat_drop, 3, 0);
    lv_obj_set_style_bg_color(sweat_drop, lv_color_hex(0x00BFFF), 0);
    lv_obj_set_style_bg_opa(sweat_drop, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sweat_drop, 0, 0);
    lv_obj_set_style_pad_all(sweat_drop, 0, 0);
    lv_obj_clear_flag(sweat_drop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sweat_drop, LV_ALIGN_TOP_RIGHT, -6, 8);
    lv_obj_add_flag(sweat_drop, LV_OBJ_FLAG_HIDDEN);

    // ── 心形（恋爱，用两个小方块模拟） ──────────────────
    heart_l = lv_obj_create(cockpit_glass);
    lv_obj_set_size(heart_l, 10, 10);
    lv_obj_set_style_radius(heart_l, 2, 0);
    lv_obj_set_style_bg_color(heart_l, C_PINK, 0);
    lv_obj_set_style_bg_opa(heart_l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(heart_l, 0, 0);
    lv_obj_set_style_pad_all(heart_l, 0, 0);
    lv_obj_clear_flag(heart_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(heart_l, LV_ALIGN_TOP_LEFT, 4, 4);
    lv_obj_add_flag(heart_l, LV_OBJ_FLAG_HIDDEN);

    heart_r = lv_obj_create(cockpit_glass);
    lv_obj_set_size(heart_r, 10, 10);
    lv_obj_set_style_radius(heart_r, 2, 0);
    lv_obj_set_style_bg_color(heart_r, C_PINK, 0);
    lv_obj_set_style_bg_opa(heart_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(heart_r, 0, 0);
    lv_obj_set_style_pad_all(heart_r, 0, 0);
    lv_obj_clear_flag(heart_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(heart_r, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_add_flag(heart_r, LV_OBJ_FLAG_HIDDEN);

    // ── 星星（开心用） ────────────────────────────────────
    star_l = lv_obj_create(cockpit_glass);
    lv_obj_set_size(star_l, 8, 8);
    lv_obj_set_style_radius(star_l, 1, 0);
    lv_obj_set_style_bg_color(star_l, C_YELLOW, 0);
    lv_obj_set_style_bg_opa(star_l, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(star_l, 0, 0);
    lv_obj_set_style_pad_all(star_l, 0, 0);
    lv_obj_clear_flag(star_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(star_l, LV_ALIGN_TOP_LEFT, 6, 6);
    lv_obj_add_flag(star_l, LV_OBJ_FLAG_HIDDEN);

    star_r = lv_obj_create(cockpit_glass);
    lv_obj_set_size(star_r, 8, 8);
    lv_obj_set_style_radius(star_r, 1, 0);
    lv_obj_set_style_bg_color(star_r, C_YELLOW, 0);
    lv_obj_set_style_bg_opa(star_r, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(star_r, 0, 0);
    lv_obj_set_style_pad_all(star_r, 0, 0);
    lv_obj_clear_flag(star_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(star_r, LV_ALIGN_TOP_RIGHT, -6, 6);
    lv_obj_add_flag(star_r, LV_OBJ_FLAG_HIDDEN);

    // ── ZZZ 标签（睡觉）─────────────────────────────────
    zzz_label = lv_label_create(cockpit_glass);
    lv_obj_set_style_text_font(zzz_label, font_sm, 0);
    lv_obj_set_style_text_color(zzz_label, C_DIM_BLUE, 0);
    lv_label_set_text(zzz_label, "z z z");
    lv_obj_align(zzz_label, LV_ALIGN_TOP_MID, 12, 4);
    lv_obj_add_flag(zzz_label, LV_OBJ_FLAG_HIDDEN);

    // ── ！标签（惊讶）───────────────────────────────────
    exclaim = lv_label_create(cockpit_glass);
    lv_obj_set_style_text_font(exclaim, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(exclaim, C_YELLOW, 0);
    lv_label_set_text(exclaim, "!!");
    lv_obj_align(exclaim, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_add_flag(exclaim, LV_OBJ_FLAG_HIDDEN);
}

// ═══════════════════════════════════════════════════════════
//  状态表情动画 (2/3)
// ═══════════════════════════════════════════════════════════

// ── 睡觉：眼睛半闭横线，ZZZ飘出，呼吸光 ──────────────────
static void expr_sleeping(void)
{
    set_cockpit_glow(C_DIM_BLUE);
    // 眼睛变横线
    lv_obj_set_size(pilot_eye_l, 10, 2);
    lv_obj_set_size(pilot_eye_r, 10, 2);
    lv_obj_set_style_radius(pilot_eye_l, 1, 0);
    lv_obj_set_style_radius(pilot_eye_r, 1, 0);
    set_eye_color(lv_color_hex(0x3A2A1A));
    // 嘴巴：微笑弧（加宽加高）
    lv_obj_set_size(pilot_mouth, 16, 5);
    lv_obj_set_style_radius(pilot_mouth, 3, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x4A2A1A), 0);
    // ZZZ 显示
    lv_obj_clear_flag(zzz_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(zzz_label, lv_color_hex(0x3A5A7A), 0);
    // 呼吸动画
    make_inf(cockpit_glass, anim_border_opa_cb, LV_OPA_20, LV_OPA_70, 2000, 2000);
    make_inf(cockpit_outer, anim_border_opa_cb, LV_OPA_10, LV_OPA_40, 2000, 2000);
    // ZZZ 上浮
    make_inf(zzz_label, anim_y_cb, 4, -8, 3000, 1000);
    make_inf(zzz_label, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 3000, 500);
}

// ── 联网中：扫描线扫过，橙色座舱，眼睛左右转 ──────────────
static void expr_connecting(void)
{
    set_cockpit_glow(C_ORANGE);
    lv_obj_set_style_border_color(thruster_l, C_ORANGE, 0);
    lv_obj_set_style_border_color(thruster_r, C_ORANGE, 0);
    set_eye_color(C_ORANGE);
    // 扫描线
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(scan_line, C_ORANGE, 0);
    make_inf(scan_line, anim_y_cb, 0, 70, 800, 200);
    // 眼睛左右扫
    make_inf(pilot_eye_l, anim_x_cb,
             lv_obj_get_x(pilot_eye_l)-6, lv_obj_get_x(pilot_eye_l)+6, 600, 600);
    make_inf(pilot_eye_r, anim_x_cb,
             lv_obj_get_x(pilot_eye_r)-6, lv_obj_get_x(pilot_eye_r)+6, 600, 600);
    // 座舱闪烁
    make_inf(cockpit_outer, anim_border_opa_cb, LV_OPA_40, LV_OPA_100, 400, 400);
}

// ── 已连接：青色，眼睛弹跳一下 ────────────────────────────
static void expr_connected(void)
{
    set_cockpit_glow(C_CYAN);
    set_eye_color(lv_color_hex(0x004488));
    lv_obj_set_size(pilot_mouth, 18, 5);
    lv_obj_set_style_radius(pilot_mouth, 2, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x2A3A2A), 0);
    // 眼睛弹跳
    make_once(pilot_eye_l, anim_h_cb, 8, 12, 150, 150);
    make_once(pilot_eye_r, anim_h_cb, 8, 12, 150, 150);
    make_inf(cockpit_glass, anim_border_opa_cb, LV_OPA_60, LV_OPA_100, 600, 600);
}

// ── 网络挂了：红色，皱眉，抖动 ────────────────────────────
static void expr_error(void)
{
    set_cockpit_glow(C_RED);
    lv_obj_set_style_border_color(thruster_l, C_RED, 0);
    lv_obj_set_style_border_color(thruster_r, C_RED, 0);
    set_eye_color(C_RED);
    // 皱眉（眉毛向中间压）
    lv_obj_set_x(pilot_brow_l, 7);
    lv_obj_set_x(pilot_brow_r, 21);
    lv_obj_set_y(pilot_brow_l, 6);
    lv_obj_set_y(pilot_brow_r, 6);
    // 嘴巴：直线皱嘴
    lv_obj_set_size(pilot_mouth, 12, 3);
    lv_obj_set_style_bg_color(pilot_mouth, C_RED, 0);
    // 抖动
    make_inf(cockpit_outer, anim_x_cb,
             lv_obj_get_x(cockpit_outer)-3, lv_obj_get_x(cockpit_outer)+3, 80, 80);
    make_inf(cockpit_outer, anim_border_opa_cb, LV_OPA_60, LV_OPA_100, 200, 200);
}

// ── 思考中：紫色，眼睛斜上看，座舱脉动 ────────────────────
static void expr_thinking(void)
{
    set_cockpit_glow(C_PURPLE);
    set_eye_color(C_PURPLE);
    // 眼睛向右上偏移（看向空处）
    lv_obj_align(pilot_eye_l, LV_ALIGN_CENTER, -7, -7);
    lv_obj_align(pilot_eye_r, LV_ALIGN_CENTER, 11, -7);
    // 眉毛微皱（一侧）
    lv_obj_set_y(pilot_brow_l, 7);
    // 嘴巴：歪嘴
    lv_obj_set_size(pilot_mouth, 10, 3);
    lv_obj_align(pilot_mouth, LV_ALIGN_CENTER, 3, 8);
    // 座舱脉动
    make_inf(cockpit_glass, anim_border_opa_cb, LV_OPA_50, LV_OPA_100, 700, 700);
    make_inf(cockpit_outer, anim_border_opa_cb, LV_OPA_40, LV_OPA_90, 700, 700);
    // 推进器闪烁
    make_inf(thruster_l, anim_opa_cb, LV_OPA_40, LV_OPA_100, 500, 500);
    make_inf(thruster_r, anim_opa_cb, LV_OPA_40, LV_OPA_100, 500, 500);
}

// ── 在码字：青色，眼睛快速左右扫，手臂抖 ─────────────────
static void expr_speaking(void)
{
    set_cockpit_glow(C_CYAN);
    set_eye_color(lv_color_hex(0x004466));
    // 嘴巴：咧嘴（宽）
    lv_obj_set_size(pilot_mouth, 20, 4);
    lv_obj_set_style_radius(pilot_mouth, 2, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x1A3A2A), 0);
    // 眼睛快速扫
    make_inf(pilot_eye_l, anim_x_cb,
             lv_obj_get_x(pilot_eye_l)-8, lv_obj_get_x(pilot_eye_l)+8, 200, 50);
    make_inf(pilot_eye_r, anim_x_cb,
             lv_obj_get_x(pilot_eye_r)-8, lv_obj_get_x(pilot_eye_r)+8, 200, 50);
    // 手臂抖动（打字感）
    make_inf(mecha_arm_l, anim_y_cb,
             lv_obj_get_y(mecha_arm_l)-3, lv_obj_get_y(mecha_arm_l)+3, 150, 150);
    make_inf(mecha_arm_r, anim_y_cb,
             lv_obj_get_y(mecha_arm_r)-3, lv_obj_get_y(mecha_arm_r)+3, 150, 150);
}

// ── 在听：绿色，大眼，头部轻点 ────────────────────────────
static void expr_listening(void)
{
    set_cockpit_glow(C_GREEN);
    set_eye_color(C_GREEN);
    lv_obj_set_size(pilot_eye_l, 10, 10);
    lv_obj_set_size(pilot_eye_r, 10, 10);
    lv_obj_set_style_radius(pilot_eye_l, 5, 0);
    lv_obj_set_style_radius(pilot_eye_r, 5, 0);
    // 嘴：O形
    lv_obj_set_size(pilot_mouth, 8, 8);
    lv_obj_set_style_radius(pilot_mouth, 4, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x1A2A1A), 0);
    // 头部轻点
    make_inf(pilot_head, anim_y_cb,
             lv_obj_get_y(pilot_head)-2, lv_obj_get_y(pilot_head)+2, 500, 500);
}

// ── 开心：黄色，弯眼，大嘴笑，星星 ────────────────────────
static void expr_happy(void)
{
    set_cockpit_glow(C_YELLOW);
    set_eye_color(C_YELLOW);
    lv_obj_set_size(pilot_eye_l, 10, 5);
    lv_obj_set_size(pilot_eye_r, 10, 5);
    lv_obj_set_style_radius(pilot_eye_l, 5, 0);
    lv_obj_set_style_radius(pilot_eye_r, 5, 0);
    // 嘴巴：大笑
    lv_obj_set_size(pilot_mouth, 22, 7);
    lv_obj_set_style_radius(pilot_mouth, 4, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x3A2000), 0);
    // 星星
    lv_obj_clear_flag(star_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(star_r, LV_OBJ_FLAG_HIDDEN);
    make_inf(star_l, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 400, 400);
    make_inf(star_r, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 600, 600);
    // 上下跳
    make_inf(cockpit_outer, anim_y_cb,
             lv_obj_get_y(cockpit_outer)-4, lv_obj_get_y(cockpit_outer)+4, 400, 400);
}

// ── 神游：座舱半透明，眼睛看向远方 ────────────────────────
static void expr_daydream(void)
{
    set_cockpit_glow(lv_color_hex(0x4080C0));
    lv_obj_set_style_bg_opa(cockpit_glass, LV_OPA_60, 0);
    set_eye_color(lv_color_hex(0x1A3A6A));
    // 眼睛看向右上
    lv_obj_align(pilot_eye_l, LV_ALIGN_CENTER, -5, -7);
    lv_obj_align(pilot_eye_r, LV_ALIGN_CENTER, 13, -7);
    lv_obj_set_size(pilot_eye_l, 6, 6);
    lv_obj_set_size(pilot_eye_r, 6, 6);
    // 嘴：微笑
    lv_obj_set_size(pilot_mouth, 12, 4);
    lv_obj_set_style_radius(pilot_mouth, 2, 0);
    // 缓慢漂移
    make_inf(cockpit_outer, anim_x_cb,
             lv_obj_get_x(cockpit_outer)-5, lv_obj_get_x(cockpit_outer)+5, 3000, 3000);
    make_inf(cockpit_glass, anim_border_opa_cb, LV_OPA_30, LV_OPA_70, 2500, 2500);
}

// ── 贪恋爱：粉色，心形眼，心形装饰 ────────────────────────
static void expr_in_love(void)
{
    set_cockpit_glow(C_PINK);
    lv_obj_set_style_border_color(thruster_l, C_PINK, 0);
    lv_obj_set_style_border_color(thruster_r, C_PINK, 0);
    set_eye_color(C_PINK);
    lv_obj_set_size(pilot_eye_l, 9, 9);
    lv_obj_set_size(pilot_eye_r, 9, 9);
    lv_obj_set_style_radius(pilot_eye_l, 2, 0);  // 方形模拟心形
    lv_obj_set_style_radius(pilot_eye_r, 2, 0);
    // 嘴：O形
    lv_obj_set_size(pilot_mouth, 9, 9);
    lv_obj_set_style_radius(pilot_mouth, 5, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x4A1A2A), 0);
    // 心形装饰
    lv_obj_clear_flag(heart_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(heart_r, LV_OBJ_FLAG_HIDDEN);
    make_inf(heart_l, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 500, 500);
    make_inf(heart_r, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 700, 700);
    make_inf(heart_l, anim_y_cb, 4, -4, 600, 600);
    make_inf(heart_r, anim_y_cb, 4, -4, 800, 800);
    // 心跳
    make_inf(cockpit_outer, anim_y_cb,
             lv_obj_get_y(cockpit_outer)-3, lv_obj_get_y(cockpit_outer)+3, 500, 500);
}

// ── 吃饭：橙色，咀嚼嘴巴，开心眼 ─────────────────────────
static void expr_eating(void)
{
    set_cockpit_glow(C_ORANGE);
    set_eye_color(lv_color_hex(0x3A2000));
    lv_obj_set_size(pilot_eye_l, 8, 5);
    lv_obj_set_size(pilot_eye_r, 8, 5);
    // 嘴巴咀嚼
    lv_obj_set_size(pilot_mouth, 14, 6);
    lv_obj_set_style_radius(pilot_mouth, 3, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x4A2000), 0);
    make_inf(pilot_mouth, anim_h_cb, 3, 8, 300, 300);
    make_inf(pilot_mouth, anim_w_cb, 10, 16, 300, 300);
    // 上下点头（看食物）
    make_inf(pilot_head, anim_y_cb,
             lv_obj_get_y(pilot_head)-2, lv_obj_get_y(pilot_head)+2, 400, 400);
}

// ── 健身：绿色，咬牙，手臂上举，汗珠 ─────────────────────
static void expr_workout(void)
{
    set_cockpit_glow(C_LIME);
    set_eye_color(C_LIME);
    lv_obj_set_size(pilot_eye_l, 8, 4);  // 眯眼
    lv_obj_set_size(pilot_eye_r, 8, 4);
    lv_obj_set_style_radius(pilot_eye_l, 2, 0);
    lv_obj_set_style_radius(pilot_eye_r, 2, 0);
    // 皱眉用力
    lv_obj_set_y(pilot_brow_l, 6);
    lv_obj_set_y(pilot_brow_r, 6);
    // 嘴：咬牙（锯齿效果，用高矮模拟）
    lv_obj_set_size(pilot_mouth, 16, 4);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x1A3A0A), 0);
    // 手臂上举
    lv_obj_align(mecha_arm_l, LV_ALIGN_CENTER, -58, 2);
    lv_obj_align(mecha_arm_r, LV_ALIGN_CENTER, 58, 2);
    // 抖动
    make_inf(cockpit_outer, anim_y_cb,
             lv_obj_get_y(cockpit_outer)-2, lv_obj_get_y(cockpit_outer)+2, 100, 100);
    // 汗珠
    lv_obj_clear_flag(sweat_drop, LV_OBJ_FLAG_HIDDEN);
    make_inf(sweat_drop, anim_y_cb, 8, 30, 1000, 200);
    make_inf(sweat_drop, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 1000, 200);
}

// ── 学习：蓝色，眼睛向左右扫（看书），专注眉 ──────────────
static void expr_studying(void)
{
    set_cockpit_glow(C_BLUE);
    set_eye_color(C_BLUE);
    lv_obj_set_size(pilot_eye_l, 8, 7);
    lv_obj_set_size(pilot_eye_r, 8, 7);
    lv_obj_align(pilot_eye_l, LV_ALIGN_CENTER, -9, -6);
    lv_obj_align(pilot_eye_r, LV_ALIGN_CENTER, 9, -6);
    // 专注眉
    lv_obj_set_y(pilot_brow_l, 6);
    lv_obj_set_y(pilot_brow_r, 6);
    // 嘴：直线专注
    lv_obj_set_size(pilot_mouth, 10, 2);
    // 眼睛左右扫（读书）
    make_inf(pilot_eye_l, anim_x_cb,
             lv_obj_get_x(pilot_eye_l)-5, lv_obj_get_x(pilot_eye_l)+5, 1500, 1500);
    make_inf(pilot_eye_r, anim_x_cb,
             lv_obj_get_x(pilot_eye_r)-5, lv_obj_get_x(pilot_eye_r)+5, 1500, 1500);
    make_inf(cockpit_glass, anim_border_opa_cb, LV_OPA_60, LV_OPA_100, 1000, 1000);
}

// ── 看电视：白色大眼，目瞪口呆，屏幕反光 ─────────────────
static void expr_watching_tv(void)
{
    set_cockpit_glow(C_WHITE);
    set_eye_color(C_WHITE);
    lv_obj_set_size(pilot_eye_l, 12, 12);
    lv_obj_set_size(pilot_eye_r, 12, 12);
    lv_obj_set_style_radius(pilot_eye_l, 6, 0);
    lv_obj_set_style_radius(pilot_eye_r, 6, 0);
    // 嘴：张开
    lv_obj_set_size(pilot_mouth, 10, 8);
    lv_obj_set_style_radius(pilot_mouth, 4, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x2A1A0A), 0);
    // 屏幕反光闪烁
    make_inf(cockpit_glass, anim_opa_cb, LV_OPA_80, LV_OPA_COVER, 1500, 100);
    // 眼睛偶尔左右跳（跟着画面）
    make_inf(pilot_eye_l, anim_x_cb,
             lv_obj_get_x(pilot_eye_l)-8, lv_obj_get_x(pilot_eye_l)+8, 1200, 100);
    make_inf(pilot_eye_r, anim_x_cb,
             lv_obj_get_x(pilot_eye_r)-8, lv_obj_get_x(pilot_eye_r)+8, 1200, 100);
}

// ── 不想理你：灰色，斜眼，嘟嘴，转头 ─────────────────────
static void expr_ignoring(void)
{
    set_cockpit_glow(C_GRAY);
    set_eye_color(C_GRAY);
    lv_obj_set_size(pilot_eye_l, 8, 4);
    lv_obj_set_size(pilot_eye_r, 8, 4);
    lv_obj_set_style_radius(pilot_eye_l, 2, 0);
    lv_obj_set_style_radius(pilot_eye_r, 2, 0);
    // 眼睛看向一侧
    lv_obj_align(pilot_eye_l, LV_ALIGN_CENTER, -5, -2);
    lv_obj_align(pilot_eye_r, LV_ALIGN_CENTER, 13, -2);
    // 嘟嘴
    lv_obj_set_size(pilot_mouth, 8, 5);
    lv_obj_set_style_radius(pilot_mouth, 3, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x303030), 0);
    // 座舱缓慢转头
    make_inf(cockpit_outer, anim_x_cb,
             lv_obj_get_x(cockpit_outer)-6, lv_obj_get_x(cockpit_outer)+6, 4000, 4000);
}

// ── 生气：红色，皱眉，咬牙，剧烈抖动 ─────────────────────
static void expr_angry(void)
{
    set_cockpit_glow(C_RED);
    lv_obj_set_style_border_color(thruster_l, C_RED, 0);
    lv_obj_set_style_border_color(thruster_r, C_RED, 0);
    set_eye_color(C_RED);
    lv_obj_set_size(pilot_eye_l, 9, 5);
    lv_obj_set_size(pilot_eye_r, 9, 5);
    lv_obj_set_style_radius(pilot_eye_l, 1, 0);
    lv_obj_set_style_radius(pilot_eye_r, 1, 0);
    // 眉毛大幅皱（内侧压低）
    lv_obj_set_y(pilot_brow_l, 5);
    lv_obj_set_y(pilot_brow_r, 5);
    lv_obj_set_x(pilot_brow_l, 8);
    lv_obj_set_x(pilot_brow_r, 20);
    // 嘴：咬牙
    lv_obj_set_size(pilot_mouth, 18, 4);
    lv_obj_set_style_bg_color(pilot_mouth, C_RED, 0);
    // 剧烈抖动
    make_inf(cockpit_outer, anim_x_cb,
             lv_obj_get_x(cockpit_outer)-5, lv_obj_get_x(cockpit_outer)+5, 60, 60);
    make_inf(cockpit_outer, anim_border_opa_cb, LV_OPA_70, LV_OPA_100, 150, 150);
}

// ── 惊讶：白色超大眼，叹号，弹起 ─────────────────────────
static void expr_surprised(void)
{
    set_cockpit_glow(C_YELLOW);
    set_eye_color(C_WHITE);
    lv_obj_set_size(pilot_eye_l, 13, 13);
    lv_obj_set_size(pilot_eye_r, 13, 13);
    lv_obj_set_style_radius(pilot_eye_l, 7, 0);
    lv_obj_set_style_radius(pilot_eye_r, 7, 0);
    // 嘴：大O
    lv_obj_set_size(pilot_mouth, 12, 12);
    lv_obj_set_style_radius(pilot_mouth, 6, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x2A1000), 0);
    // 叹号
    lv_obj_clear_flag(exclaim, LV_OBJ_FLAG_HIDDEN);
    // 弹起
    make_once(cockpit_outer, anim_y_cb,
              lv_obj_get_y(cockpit_outer), lv_obj_get_y(cockpit_outer)-10, 120, 200);
    make_inf(cockpit_outer, anim_border_opa_cb, LV_OPA_80, LV_OPA_100, 300, 300);
}

// ── 无聊：灰色，眼皮半闭，嘴角下撇 ────────────────────────
static void expr_bored(void)
{
    set_cockpit_glow(lv_color_hex(0x404050));
    set_eye_color(lv_color_hex(0x404050));
    lv_obj_set_size(pilot_eye_l, 8, 3);
    lv_obj_set_size(pilot_eye_r, 8, 3);
    lv_obj_set_style_radius(pilot_eye_l, 1, 0);
    lv_obj_set_style_radius(pilot_eye_r, 1, 0);
    // 嘴：下撇
    lv_obj_set_size(pilot_mouth, 14, 3);
    lv_obj_align(pilot_mouth, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x303040), 0);
    // 极慢漂移
    make_inf(cockpit_outer, anim_y_cb,
             lv_obj_get_y(cockpit_outer)-3, lv_obj_get_y(cockpit_outer)+3, 4000, 4000);
    make_inf(cockpit_glass, anim_border_opa_cb, LV_OPA_40, LV_OPA_60, 2000, 2000);
}

// ── 赛博模式：荧光绿，方形眼，高速扫描 ────────────────────
static void expr_cyber(void)
{
    set_cockpit_glow(C_LIME);
    lv_obj_set_style_bg_color(cockpit_glass, lv_color_hex(0x001400), 0);
    set_eye_color(C_LIME);
    lv_obj_set_style_radius(pilot_eye_l, 1, 0);
    lv_obj_set_style_radius(pilot_eye_r, 1, 0);
    lv_obj_set_size(pilot_eye_l, 10, 5);
    lv_obj_set_size(pilot_eye_r, 10, 5);
    // 嘴：锯齿（矩形）
    lv_obj_set_size(pilot_mouth, 16, 3);
    lv_obj_set_style_radius(pilot_mouth, 0, 0);
    lv_obj_set_style_bg_color(pilot_mouth, C_LIME, 0);
    // 扫描线（绿色）
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(scan_line, C_LIME, 0);
    make_inf(scan_line, anim_y_cb, 0, 70, 150, 50);
    // 眼睛闪烁
    make_inf(pilot_eye_l, anim_opa_cb, LV_OPA_50, LV_OPA_COVER, 100, 100);
    make_inf(pilot_eye_r, anim_opa_cb, LV_OPA_50, LV_OPA_COVER, 100, 100);
    // 座舱快闪
    make_inf(cockpit_outer, anim_border_opa_cb, LV_OPA_60, LV_OPA_100, 80, 80);
}

// ── 发晕：黄色，螺旋眼（用大小脉动），摇晃 ────────────────
static void expr_dizzy(void)
{
    set_cockpit_glow(C_YELLOW);
    set_eye_color(C_YELLOW);
    lv_obj_set_size(pilot_eye_l, 10, 10);
    lv_obj_set_size(pilot_eye_r, 10, 10);
    lv_obj_set_style_radius(pilot_eye_l, 2, 0);
    lv_obj_set_style_radius(pilot_eye_r, 2, 0);
    // 瞳孔脉动（模拟螺旋）
    make_inf(pilot_eye_l, anim_w_cb, 4, 10, 300, 300);
    make_inf(pilot_eye_r, anim_w_cb, 4, 10, 300, 300);
    make_inf(pilot_eye_l, anim_h_cb, 4, 10, 300, 300);
    make_inf(pilot_eye_r, anim_h_cb, 4, 10, 300, 300);
    // 嘴：弯曲
    lv_obj_set_size(pilot_mouth, 12, 4);
    lv_obj_set_style_radius(pilot_mouth, 3, 0);
    // 座舱摇晃
    make_inf(cockpit_outer, anim_x_cb,
             lv_obj_get_x(cockpit_outer)-8, lv_obj_get_x(cockpit_outer)+8, 250, 250);
}

// ── 害羞：粉色，低头，腮红，汗珠 ─────────────────────────
static void expr_shy(void)
{
    set_cockpit_glow(C_PINK);
    set_eye_color(lv_color_hex(0x4A1A2A));
    lv_obj_set_size(pilot_eye_l, 6, 4);
    lv_obj_set_size(pilot_eye_r, 6, 4);
    // 眼睛向下看
    lv_obj_align(pilot_eye_l, LV_ALIGN_CENTER, -9, 0);
    lv_obj_align(pilot_eye_r, LV_ALIGN_CENTER, 9, 0);
    // 嘴：小微笑
    lv_obj_set_size(pilot_mouth, 10, 4);
    lv_obj_set_style_radius(pilot_mouth, 2, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x4A2030), 0);
    // 腮红
    lv_obj_clear_flag(pilot_blush_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pilot_blush_r, LV_OBJ_FLAG_HIDDEN);
    make_inf(pilot_blush_l, anim_opa_cb, LV_OPA_30, LV_OPA_70, 800, 800);
    make_inf(pilot_blush_r, anim_opa_cb, LV_OPA_30, LV_OPA_70, 800, 800);
    // 汗珠
    lv_obj_clear_flag(sweat_drop, LV_OBJ_FLAG_HIDDEN);
    make_inf(sweat_drop, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 1200, 200);
    // 微微低头
    make_inf(pilot_head, anim_y_cb,
             lv_obj_get_y(pilot_head), lv_obj_get_y(pilot_head)+3, 1000, 1000);
}

// ── 亢奋：金色，大眼疯狂闪烁，机甲跳动 ────────────────────
static void expr_excited(void)
{
    set_cockpit_glow(C_GOLD);
    lv_obj_set_style_border_color(thruster_l, C_GOLD, 0);
    lv_obj_set_style_border_color(thruster_r, C_GOLD, 0);
    set_eye_color(C_GOLD);
    lv_obj_set_size(pilot_eye_l, 12, 12);
    lv_obj_set_size(pilot_eye_r, 12, 12);
    lv_obj_set_style_radius(pilot_eye_l, 6, 0);
    lv_obj_set_style_radius(pilot_eye_r, 6, 0);
    // 嘴：咧嘴大笑
    lv_obj_set_size(pilot_mouth, 22, 7);
    lv_obj_set_style_radius(pilot_mouth, 4, 0);
    lv_obj_set_style_bg_color(pilot_mouth, lv_color_hex(0x3A2800), 0);
    // 眼睛快速闪烁
    make_inf(pilot_eye_l, anim_opa_cb, LV_OPA_50, LV_OPA_COVER, 150, 150);
    make_inf(pilot_eye_r, anim_opa_cb, LV_OPA_50, LV_OPA_COVER, 150, 150);
    // 整体快速跳动
    make_inf(cockpit_outer, anim_y_cb,
             lv_obj_get_y(cockpit_outer)-6, lv_obj_get_y(cockpit_outer)+6, 200, 200);
    make_inf(mecha_arm_l, anim_y_cb,
             lv_obj_get_y(mecha_arm_l)-4, lv_obj_get_y(mecha_arm_l)+4, 200, 200);
    make_inf(mecha_arm_r, anim_y_cb,
             lv_obj_get_y(mecha_arm_r)-4, lv_obj_get_y(mecha_arm_r)+4, 200, 200);
}

// ── 配置模式：等待色，扫描线 ───────────────────────────────
static void expr_config(void)
{
    set_cockpit_glow(C_CYAN);
    set_eye_color(lv_color_hex(0x004466));
    lv_obj_set_size(pilot_eye_l, 6, 6);
    lv_obj_set_size(pilot_eye_r, 6, 6);
    lv_obj_set_size(pilot_mouth, 10, 3);
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(scan_line, C_CYAN, 0);
    make_inf(scan_line, anim_y_cb, 0, 70, 1200, 300);
    make_inf(cockpit_glass, anim_border_opa_cb, LV_OPA_40, LV_OPA_100, 800, 800);
}

// ═══════════════════════════════════════════════════════════
//  状态切换主函数
// ═══════════════════════════════════════════════════════════

static void update_state_display(lcd_state_t state)
{
    stop_all_anims();
    hide_all_decorations();
    reset_face();

    // 恢复座舱玻璃背景
    lv_obj_set_style_bg_color(cockpit_glass, C_COCKPIT_GLASS, 0);
    lv_obj_set_style_bg_opa(cockpit_glass, LV_OPA_COVER, 0);
    // 恢复推进器颜色
    lv_obj_set_style_border_color(thruster_l, C_CYAN, 0);
    lv_obj_set_style_border_color(thruster_r, C_CYAN, 0);

    switch (state) {
    case LCD_STATE_SLEEPING:     expr_sleeping();    break;
    case LCD_STATE_CONNECTING:   expr_connecting();  break;
    case LCD_STATE_CONNECTED:    expr_connected();   break;
    case LCD_STATE_ERROR:        expr_error();       break;
    case LCD_STATE_THINKING:     expr_thinking();    break;
    case LCD_STATE_SPEAKING:     expr_speaking();    break;
    case LCD_STATE_LISTENING:    expr_listening();   break;
    case LCD_STATE_HAPPY:        expr_happy();       break;
    case LCD_STATE_DAYDREAM:     expr_daydream();    break;
    case LCD_STATE_IN_LOVE:      expr_in_love();     break;
    case LCD_STATE_EATING:       expr_eating();      break;
    case LCD_STATE_WORKOUT:      expr_workout();     break;
    case LCD_STATE_STUDYING:     expr_studying();    break;
    case LCD_STATE_WATCHING_TV:  expr_watching_tv(); break;
    case LCD_STATE_IGNORING:     expr_ignoring();    break;
    case LCD_STATE_ANGRY:        expr_angry();       break;
    case LCD_STATE_SURPRISED:    expr_surprised();   break;
    case LCD_STATE_BORED:        expr_bored();       break;
    case LCD_STATE_CYBER:        expr_cyber();       break;
    case LCD_STATE_DIZZY:        expr_dizzy();       break;
    case LCD_STATE_SHY:          expr_shy();         break;
    case LCD_STATE_EXCITED:      expr_excited();     break;
    case LCD_STATE_CONFIG:       expr_config();      break;
    default: break;
    }

    // 更新状态文字（使用中文字体）
    if (status_label && state < LCD_STATE_COUNT) {
        lv_label_set_text(status_label, state_labels[state]);
    }

    // WiFi 图标颜色
    if (wifi_icon) {
        lv_color_t wc;
        if      (state == LCD_STATE_ERROR)      wc = C_RED;
        else if (state == LCD_STATE_CONNECTING) wc = C_ORANGE;
        else if (state == LCD_STATE_CONFIG)     wc = C_CYAN;
        else                                    wc = lv_color_hex(0x4080A0);
        lv_obj_set_style_text_color(wifi_icon, wc, 0);
    }
}

// ═══════════════════════════════════════════════════════════
//  随机情绪触发
// ═══════════════════════════════════════════════════════════
static const lcd_state_t mood_pool[] = {
    LCD_STATE_HAPPY, LCD_STATE_DAYDREAM, LCD_STATE_IN_LOVE,
    LCD_STATE_EATING, LCD_STATE_BORED, LCD_STATE_SHY,
    LCD_STATE_EXCITED, LCD_STATE_WATCHING_TV, LCD_STATE_DIZZY,
};
#define MOOD_POOL_SIZE (int)(sizeof(mood_pool)/sizeof(mood_pool[0]))

static void mood_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (current_state != LCD_STATE_CONNECTED &&
        current_state != LCD_STATE_SLEEPING) return;

    mood_ticks++;
    if (mood_ticks < 30) return;
    mood_ticks = 0;

    uint32_t r = (uint32_t)(esp_timer_get_time() & 0xFFFF);
    if ((r % 10) >= 3) return;

    lcd_state_t prev  = current_state;
    lcd_state_t mood  = mood_pool[r % MOOD_POOL_SIZE];

    if (lvgl_port_lock(0)) {
        update_state_display(mood);
        lvgl_port_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(4000));
    if (lvgl_port_lock(0)) {
        current_state = prev;
        update_state_display(prev);
        lvgl_port_unlock();
    }
}

// 定时眨眼
static void blink_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (current_state == LCD_STATE_SLEEPING ||
        current_state == LCD_STATE_CONNECTING ||
        current_state == LCD_STATE_CONFIG) return;
    if (!pilot_eye_l || !pilot_eye_r) return;
    if (lvgl_port_lock(0)) {
        make_once(pilot_eye_l, anim_h_cb, lv_obj_get_height(pilot_eye_l), 1, 80, 80);
        make_once(pilot_eye_r, anim_h_cb, lv_obj_get_height(pilot_eye_r), 1, 80, 80);
        lvgl_port_unlock();
    }
}

// ═══════════════════════════════════════════════════════════
//  流式文字（typewriter 效果）(3/3)
// ═══════════════════════════════════════════════════════════

void lcd_stream_begin(bool is_assistant)
{
    if (lvgl_port_lock(500)) {
        stream_color  = is_assistant ? lv_color_hex(0xB0B0FF) : lv_color_hex(0x80FFB0);
        stream_active = true;
        stream_len    = 0;
        stream_buf[0] = '\0';

        // 创建新的流式 label
        stream_label = lv_label_create(mouth_area);
        lv_obj_set_style_text_font(stream_label, font_cn, 0);
        lv_obj_set_style_text_color(stream_label, stream_color, 0);
        lv_obj_set_width(stream_label, LCD_H_RES - 44);
        lv_label_set_long_mode(stream_label, LV_LABEL_LONG_WRAP);
        lv_label_set_text(stream_label, "");

        // 限制对话区消息数量
        uint32_t cnt = lv_obj_get_child_count(mouth_area);
        if (cnt > 18) {
            lv_obj_t *first = lv_obj_get_child(mouth_area, 0);
            if (first) lv_obj_delete(first);
        }
        lvgl_port_unlock();
    }
}

void lcd_stream_append(const char *chunk)
{
    if (!chunk || !stream_active) return;

    size_t clen = strlen(chunk);
    if (stream_len + (int)clen >= STREAM_BUF_SIZE - 1) {
        // 缓冲区满，截断
        clen = (size_t)(STREAM_BUF_SIZE - 1 - stream_len);
    }
    if (clen == 0) return;

    memcpy(stream_buf + stream_len, chunk, clen);
    stream_len += (int)clen;
    stream_buf[stream_len] = '\0';

    if (lvgl_port_lock(200)) {
        if (stream_label) {
            lv_label_set_text(stream_label, stream_buf);
            lv_obj_scroll_to_view_recursive(stream_label, LV_ANIM_OFF);
        }
        lvgl_port_unlock();
    }
}

void lcd_stream_end(void)
{
    stream_active = false;
    stream_label  = NULL;
    stream_len    = 0;
    stream_buf[0] = '\0';
}

// ═══════════════════════════════════════════════════════════
//  QR 码覆盖层（使用 LVGL canvas 逐点绘制）
// ═══════════════════════════════════════════════════════════

// 简易 QR 码矩阵：使用 LVGL 小方块拼接
// 外部通过 lcd_show_qr_overlay 传入预生成的矩阵
// 为节省 FLASH，QR 渲染由 config_portal 调用时传入 bit 数组

void lcd_show_qr_overlay(const char *url, const char *hint)
{
    (void)url;  // URL 由上层已生成好矩阵
    if (!lvgl_port_lock(500)) return;

    // 如果已有覆盖层，先删除
    if (qr_overlay) {
        lv_obj_delete(qr_overlay);
        qr_overlay = NULL;
        qr_canvas  = NULL;
    }

    // 创建半透明覆盖层
    qr_overlay = lv_obj_create(screen);
    lv_obj_set_size(qr_overlay, LCD_H_RES, LCD_V_RES);
    lv_obj_align(qr_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(qr_overlay, lv_color_hex(0x000010), 0);
    lv_obj_set_style_bg_opa(qr_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(qr_overlay, 0, 0);
    lv_obj_set_style_pad_all(qr_overlay, 0, 0);
    lv_obj_clear_flag(qr_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // 标题
    lv_obj_t *title = lv_label_create(qr_overlay);
    lv_obj_set_style_text_font(title, font_cn, 0);
    lv_obj_set_style_text_color(title, C_CYAN, 0);
    lv_label_set_text(title, "扫码进入配置");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    // QR 码画布（lv_canvas，直接像素绘制，避免创建大量子对象）
    // 画布尺寸 152×152，像素格式 RGB565
    static lv_color_t qr_cbuf[QR_CANVAS_PX * QR_CANVAS_PX];
    qr_canvas = lv_canvas_create(qr_overlay);
    lv_canvas_set_buffer(qr_canvas, qr_cbuf, QR_CANVAS_PX, QR_CANVAS_PX, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_size(qr_canvas, QR_CANVAS_PX, QR_CANVAS_PX);
    lv_obj_align(qr_canvas, LV_ALIGN_CENTER, 0, -10);
    lv_canvas_fill_bg(qr_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);

    // 提示文字（WiFi SSID）
    lv_obj_t *hint_lbl = lv_label_create(qr_overlay);
    lv_obj_set_style_text_font(hint_lbl, font_cn, 0);
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0xA0C0FF), 0);
    lv_label_set_text(hint_lbl, hint ? hint : "http://192.168.4.1");
    lv_obj_align(hint_lbl, LV_ALIGN_BOTTOM_MID, 0, -8);

    lvgl_port_unlock();
}

// 在 qr_canvas（lv_canvas）内绘制 QR 矩阵
void lcd_draw_qr_matrix(const uint8_t *modules, int size)
{
    if (!qr_canvas || !modules) return;
    if (!lvgl_port_lock(500)) return;

    // 每个 module 的像素大小（画布 152px，留 4px 白边）
    int canvas_px = QR_CANVAS_PX - 8;  /* 144 可用 */
    int mod_px = canvas_px / size;
    if (mod_px < 1) mod_px = 1;

    // 先填白底
    lv_canvas_fill_bg(qr_canvas, lv_color_hex(0xFFFFFF), LV_OPA_COVER);

    int offset = (QR_CANVAS_PX - mod_px * size) / 2;  /* 居中 */

    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.radius   = 0;
    dsc.border_width = 0;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            bool dark = modules[y * size + x] != 0;
            lv_color_t c = dark ? lv_color_hex(0x000000) : lv_color_hex(0xFFFFFF);
            /* Fill mod_px × mod_px pixels for this module */
            int px0 = offset + x * mod_px;
            int py0 = offset + y * mod_px;
            for (int dy = 0; dy < mod_px; dy++)
                for (int dx = 0; dx < mod_px; dx++)
                    lv_canvas_set_px(qr_canvas, px0 + dx, py0 + dy, c, LV_OPA_COVER);
        }
    }

    lvgl_port_unlock();
}

void lcd_hide_qr_overlay(void)
{
    if (!lvgl_port_lock(500)) return;
    if (qr_overlay) {
        lv_obj_delete(qr_overlay);
        qr_overlay = NULL;
        qr_canvas  = NULL;
    }
    lvgl_port_unlock();
}

// ═══════════════════════════════════════════════════════════
//  对话区工具函数
// ═══════════════════════════════════════════════════════════

static void add_text_to_mouth(const char *prefix, const char *content, lv_color_t color)
{
    lv_obj_t *line = lv_label_create(mouth_area);
    lv_obj_set_style_text_font(line, font_cn, 0);
    lv_obj_set_style_text_color(line, color, 0);
    lv_obj_set_width(line, LCD_H_RES - 44);
    lv_label_set_long_mode(line, LV_LABEL_LONG_WRAP);

    char buf[512];
    if (prefix && prefix[0]) {
        snprintf(buf, sizeof(buf), "%s%s", prefix, content);
    } else {
        snprintf(buf, sizeof(buf), "%s", content);
    }
    lv_label_set_text(line, buf);
    lv_obj_scroll_to_view_recursive(line, LV_ANIM_ON);

    uint32_t cnt = lv_obj_get_child_count(mouth_area);
    if (cnt > 20) {
        lv_obj_t *first = lv_obj_get_child(mouth_area, 0);
        if (first) lv_obj_delete(first);
    }
}

// ═══════════════════════════════════════════════════════════
//  UI 初始化
// ═══════════════════════════════════════════════════════════

static void setup_ui(void)
{
    font_cn = &font_chinese_14;
    font_sm = &lv_font_montserrat_12;

    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, C_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    // ── 脸部区域 ────────────────────────────────────────────
    face_area = lv_obj_create(screen);
    lv_obj_set_size(face_area, LCD_H_RES, 132);
    lv_obj_align(face_area, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(face_area, C_FACE_BG, 0);
    lv_obj_set_style_bg_opa(face_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(face_area, 0, 0);
    lv_obj_set_style_radius(face_area, 0, 0);
    lv_obj_set_style_pad_all(face_area, 0, 0);
    lv_obj_set_scrollbar_mode(face_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(face_area, LV_OBJ_FLAG_SCROLLABLE);

    // 构建机甲座舱
    build_mecha_ui();

    // ── 对话区 ──────────────────────────────────────────────
    mouth_area = lv_obj_create(screen);
    lv_obj_set_size(mouth_area, LCD_H_RES - 16, LCD_V_RES - 132 - 26);
    lv_obj_align(mouth_area, LV_ALIGN_TOP_MID, 0, 136);
    lv_obj_set_style_bg_color(mouth_area, C_MOUTH_BG, 0);
    lv_obj_set_style_bg_opa(mouth_area, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mouth_area, 8, 0);
    lv_obj_set_style_border_width(mouth_area, 1, 0);
    lv_obj_set_style_border_color(mouth_area, C_MOUTH_BORDER, 0);
    lv_obj_set_style_border_opa(mouth_area, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(mouth_area, 5, 0);
    lv_obj_set_scrollbar_mode(mouth_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(mouth_area, LV_DIR_VER);
    lv_obj_set_flex_flow(mouth_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mouth_area, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(mouth_area, 3, 0);

    // 初始占位 label
    lv_obj_t *placeholder = lv_label_create(mouth_area);
    lv_obj_set_style_text_font(placeholder, font_cn, 0);
    lv_obj_set_style_text_color(placeholder, C_TEXT_DIM, 0);
    lv_label_set_text(placeholder, "");
    lv_obj_set_width(placeholder, LCD_H_RES - 44);
    lv_label_set_long_mode(placeholder, LV_LABEL_LONG_WRAP);

    // ── 底部状态栏 ──────────────────────────────────────────
    // 状态文字（使用中文字体，修复乱码）
    status_label = lv_label_create(screen);
    lv_obj_set_style_text_font(status_label, font_cn, 0);  // ← 关键：中文字体
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x6090B0), 0);
    lv_label_set_text(status_label, "在睡觉");
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -32, -5);

    // WiFi 图标
    wifi_icon = lv_label_create(screen);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x4080A0), 0);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(wifi_icon, LV_ALIGN_BOTTOM_RIGHT, -5, -5);

    // 定时器
    lv_timer_create(blink_timer_cb, 4000, NULL);
    lv_timer_create(mood_timer_cb,  1000, NULL);
}

// ═══════════════════════════════════════════════════════════
//  硬件初始化
// ═══════════════════════════════════════════════════════════

esp_err_t lcd_display_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD with LVGL (ST7789 240x240)");

    gpio_config_t bk_gpio = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio));
    lcd_backlight_set(false);

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_SCLK, .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1, .quadwp_io_num = -1, .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "SPI init failed"); return ret; }

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num = PIN_DC, .cs_gpio_num = PIN_CS,
        .pclk_hz = LCD_PIXEL_CLK, .lcd_cmd_bits = 8, .lcd_param_bits = 8,
        .spi_mode = 3, .trans_queue_depth = 10,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_cfg, &lcd_io);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "Panel IO failed"); return ret; }

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd_io, &panel_cfg, &lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(lcd_panel));
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_lcd_panel_init(lcd_panel));
    vTaskDelay(pdMS_TO_TICKS(200));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(lcd_panel, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(lcd_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(lcd_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(lcd_panel, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(lcd_panel, true));
    vTaskDelay(pdMS_TO_TICKS(100));

    lv_init();

    const lvgl_port_cfg_t port_cfg = {
        .task_priority = 4, .task_stack = 8192,
        .task_affinity = 1, .task_max_sleep_ms = 500, .timer_period_ms = 5,
    };
    ret = lvgl_port_init(&port_cfg);
    if (ret != ESP_OK) { ESP_LOGE(TAG, "LVGL port init failed"); return ret; }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io, .panel_handle = lcd_panel, .control_handle = NULL,
        .buffer_size = LCD_H_RES * 20, .double_buffer = false, .trans_size = 0,
        .hres = LCD_H_RES, .vres = LCD_V_RES, .monochrome = false,
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = { .buff_dma = 1, .buff_spiram = 0, .sw_rotate = 0,
                   .swap_bytes = 1, .full_refresh = 0, .direct_mode = 0 },
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    if (!lvgl_disp) { ESP_LOGE(TAG, "Failed to add display"); return ESP_FAIL; }

    lcd_backlight_set(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    if (lvgl_port_lock(0)) {
        setup_ui();
        update_state_display(LCD_STATE_SLEEPING);
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "LCD initialized OK");
    return ESP_OK;
}

// ═══════════════════════════════════════════════════════════
//  公开 API
// ═══════════════════════════════════════════════════════════

void lcd_set_state(lcd_state_t state)
{
    current_state = state;
    if (lvgl_port_lock(500)) {
        update_state_display(state);
        lvgl_port_unlock();
    }
}

void lcd_show_chat_message(const char *role, const char *content)
{
    if (!content || content[0] == '\0') return;
    if (lvgl_port_lock(500)) {
        bool is_user     = (strcmp(role, "user") == 0);
        lv_color_t color = is_user ? lv_color_hex(0x80FFB0) : lv_color_hex(0xB0B0FF);
        const char *pfx  = is_user ? "> " : "";
        add_text_to_mouth(pfx, content, color);
        lvgl_port_unlock();
    }
}

void lcd_clear_chat(void)
{
    if (lvgl_port_lock(500)) {
        lv_obj_clean(mouth_area);
        lv_obj_t *ph = lv_label_create(mouth_area);
        lv_obj_set_style_text_font(ph, font_cn, 0);
        lv_obj_set_style_text_color(ph, C_TEXT_DIM, 0);
        lv_label_set_text(ph, "");
        lv_obj_set_width(ph, LCD_H_RES - 44);
        lv_label_set_long_mode(ph, LV_LABEL_LONG_WRAP);
        stream_label  = NULL;
        stream_active = false;
        lvgl_port_unlock();
    }
}

void lcd_set_status_text(const char *text)
{
    if (!text) return;
    if (lvgl_port_lock(500)) {
        if (status_label) lv_label_set_text(status_label, text);
        lvgl_port_unlock();
    }
}

