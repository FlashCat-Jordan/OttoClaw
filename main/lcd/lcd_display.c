/*
 * lcd_display.c — MiaomiaoClaw 像素风机甲座舱 UI
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
#include "driver/ledc.h"
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

// ═══════════════════════════════════════════════════════════
//  颜色定义
// ═══════════════════════════════════════════════════════════
#define C_BG            lv_color_hex(0x0A0A12)  // 屏幕背景
#define C_FACE_BG       lv_color_hex(0x0A0A12)  // 机器人区背景
#define C_MOUTH_BG      lv_color_hex(0x0C0C1E)  // 对话区背景
#define C_MOUTH_BORDER  lv_color_hex(0x1A1A3A)
#define C_TEXT_USER     lv_color_hex(0x80FFB0)
#define C_TEXT_ASSIST   lv_color_hex(0xB0B0FF)
#define C_TEXT_DIM      lv_color_hex(0x505070)

// Otto 机器人颜色
#define C_ROBOT_BODY    lv_color_hex(0x1A1A22)  // 机器人主体（近黑偏蓝）
#define C_ROBOT_EDGE    lv_color_hex(0xDDDDEE)  // 白色轮廓线
#define C_ROBOT_JOINT   lv_color_hex(0x2A2A3A)  // 关节/暗部
#define C_SCREEN_BG     lv_color_hex(0x040810)  // 头部屏幕背景（极深蓝）
#define C_SCREEN_BORDER lv_color_hex(0x303060)  // 屏幕边框

// 情绪主题色
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
//  Otto 机器人部件句柄
// ═══════════════════════════════════════════════════════════
// 头部
static lv_obj_t *robot_head    = NULL;  // 头部方块
static lv_obj_t *antenna_l     = NULL;  // 左天线杆
static lv_obj_t *antenna_r     = NULL;  // 右天线杆
static lv_obj_t *antenna_ball_l = NULL; // 左天线顶球
static lv_obj_t *antenna_ball_r = NULL; // 右天线顶球
static lv_obj_t *face_screen   = NULL;  // 头部屏幕（表情显示区）
static lv_obj_t *scan_line     = NULL;  // 扫描线

// 眼睛（在 face_screen 内）
static lv_obj_t *eye_l         = NULL;
static lv_obj_t *eye_r         = NULL;
static lv_obj_t *pupil_l       = NULL;  // 瞳孔高光
static lv_obj_t *pupil_r       = NULL;
static lv_obj_t *brow_l        = NULL;  // 眉毛
static lv_obj_t *brow_r        = NULL;

// 嘴巴（在 face_screen 内）
static lv_obj_t *mouth_shape   = NULL;

// 装饰（在 face_screen 内）
static lv_obj_t *deco_l        = NULL;  // 左装饰（腮红/星/心）
static lv_obj_t *deco_r        = NULL;  // 右装饰
static lv_obj_t *zzz_label     = NULL;  // ZZZ
static lv_obj_t *exclaim       = NULL;  // ！

// 身体
static lv_obj_t *robot_root    = NULL;  // 整体容器（动画移动此对象即可）
static lv_obj_t *robot_body    = NULL;  // 躯干
static lv_obj_t *arm_l         = NULL;  // 左手臂
static lv_obj_t *arm_r         = NULL;  // 右手臂
static lv_obj_t *leg_l         = NULL;  // 左腿
static lv_obj_t *leg_r         = NULL;  // 右腿
static lv_obj_t *foot_l        = NULL;  // 左脚
static lv_obj_t *foot_r        = NULL;  // 右脚

// ═══════════════════════════════════════════════════════════
//  硬件句柄
// ═══════════════════════════════════════════════════════════
static esp_lcd_panel_io_handle_t lcd_io    = NULL;
static esp_lcd_panel_handle_t    lcd_panel = NULL;
static lv_display_t             *lvgl_disp = NULL;

// ═══════════════════════════════════════════════════════════
//  UI 根对象
// ═══════════════════════════════════════════════════════════
static lv_obj_t *screen    = NULL;
static lv_obj_t *face_area = NULL;   // 上半区 240×132，容纳机器人

// robot_root 初始位置（固定基准，防止动画累积漂移）
// 运行时由 build_robot_ui() 调用后记录真实坐标
#define ROBOT_ROOT_Y0  (-4)
static lv_coord_t s_robot_root_x0 = 79;  // 运行时更新
// arm 默认位置（build_robot_ui 中 body_y+4=83，arm_l x=0, arm_r x=72）
#define ARM_Y0  83
// eye 默认 x 位置（face_screen 内，LV_ALIGN_CENTER ±11，eye宽10）
#define EYE_L_X0  11
#define EYE_R_X0  31

// ── 配网提示覆盖层 ──
static lv_obj_t *qr_overlay   = NULL;

// ── 状态 ──
static lcd_state_t current_state = LCD_STATE_SLEEPING;
static lcd_state_t base_mood = LCD_STATE_SLEEPING;
static const lv_font_t *font_cn  = NULL;
static const lv_font_t *font_sm  = NULL;

// ── 流式缓冲 ──
#define STREAM_BUF_SIZE 4096
static char     stream_buf[STREAM_BUF_SIZE];
static int      stream_len  = 0;
static bool     stream_active = false;
static lv_color_t stream_color;

// ── 随机情绪 ──
static uint32_t mood_ticks = 0;

// ═══════════════════════════════════════════════════════════
//  工具函数
// ═══════════════════════════════════════════════════════════

static void bl_ledc_init(void)
{
    ledc_timer_config_t t = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num       = LEDC_TIMER_0,
        .freq_hz         = 20000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);

    ledc_channel_config_t c = {
        .gpio_num   = PIN_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .duty       = 0,
        .hpoint     = 0,
    };
    ledc_channel_config(&c);
}

void lcd_backlight_set(bool on)
{
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? 1023 : 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void stop_all_anims(void)
{
    lv_obj_t *objs[] = {
        robot_root, robot_head, robot_body, arm_l, arm_r, leg_l, leg_r, foot_l, foot_r,
        antenna_l, antenna_r, antenna_ball_l, antenna_ball_r,
        face_screen, scan_line,
        eye_l, eye_r, pupil_l, pupil_r, brow_l, brow_r,
        mouth_shape, deco_l, deco_r, zzz_label, exclaim
    };
    for (int i = 0; i < (int)(sizeof(objs)/sizeof(objs[0])); i++) {
        if (objs[i]) lv_anim_delete(objs[i], NULL);
    }
}

static void hide_all_decorations(void)
{
    lv_obj_t *deco[] = { deco_l, deco_r, zzz_label, exclaim, scan_line };
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
    lv_anim_set_early_apply(&a, true);
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

// 设置屏幕发光边框颜色（情绪主色）
static void set_screen_glow(lv_color_t col)
{
    if (face_screen) lv_obj_set_style_border_color(face_screen, col, 0);
    if (antenna_ball_l) lv_obj_set_style_bg_color(antenna_ball_l, col, 0);
    if (antenna_ball_r) lv_obj_set_style_bg_color(antenna_ball_r, col, 0);
}

// 设置眼睛颜色
static void set_eye_color(lv_color_t col)
{
    if (eye_l) lv_obj_set_style_bg_color(eye_l, col, 0);
    if (eye_r) lv_obj_set_style_bg_color(eye_r, col, 0);
}

// 重置眼睛到默认圆眼（8×8）
static void reset_eyes_round(void)
{
    if (eye_l) {
        lv_obj_set_size(eye_l, 10, 10);
        lv_obj_set_style_radius(eye_l, 5, 0);
        lv_obj_align(eye_l, LV_ALIGN_CENTER, -12, -4);
    }
    if (eye_r) {
        lv_obj_set_size(eye_r, 10, 10);
        lv_obj_set_style_radius(eye_r, 5, 0);
        lv_obj_align(eye_r, LV_ALIGN_CENTER, 12, -4);
    }
    set_eye_color(lv_color_hex(0x00CCFF));
}

// 重置嘴巴到默认直线
static void reset_mouth_flat(void)
{
    if (mouth_shape) {
        lv_obj_set_size(mouth_shape, 18, 3);
        lv_obj_set_style_radius(mouth_shape, 1, 0);
        lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x4488AA), 0);
        lv_obj_align(mouth_shape, LV_ALIGN_CENTER, 0, 10);
    }
}

// 重置眉毛到正常位置
static void reset_brows(void)
{
    if (brow_l) {
        lv_obj_set_size(brow_l, 10, 2);
        lv_obj_set_style_bg_color(brow_l, lv_color_hex(0x00CCFF), 0);
        lv_obj_set_style_radius(brow_l, 1, 0);
        lv_obj_align(brow_l, LV_ALIGN_CENTER, -12, -15);
    }
    if (brow_r) {
        lv_obj_set_size(brow_r, 10, 2);
        lv_obj_set_style_bg_color(brow_r, lv_color_hex(0x00CCFF), 0);
        lv_obj_set_style_radius(brow_r, 1, 0);
        lv_obj_align(brow_r, LV_ALIGN_CENTER, 12, -15);
    }
}

// 重置手臂到默认下垂（绝对坐标，与 build_robot_ui 一致）
// robot_root cx=41, body_x=cx-body_w/2=12, body_y=neck_y=head_y+head_h+neck_h=21+52+6=79
// arm_l: x=body_x-arm_w-2=0,  y=body_y+4=83
// arm_r: x=body_x+body_w+2=72,y=body_y+4=83
static void reset_arms(void)
{
    if (arm_l) lv_obj_set_pos(arm_l, 0,  83);
    if (arm_r) lv_obj_set_pos(arm_r, 72, 83);
}

// 重置腿部到默认站立（绝对坐标，与 build_robot_ui 一致）
// leg_y = body_y+body_h = 79+34=113
// foot_y= leg_y+leg_h   = 113+20=133
// leg_l:  x=cx-body_w/2+6=17,  leg_r: x=cx+body_w/2-6-leg_w=41+29-6-14=50
// foot_l: x=cx-body_w/2+2=13,  foot_r: x=cx+body_w/2-2-foot_w=41+29-2-22=46
static void reset_legs(void)
{
    if (leg_l)  lv_obj_set_pos(leg_l,  17, 113);
    if (leg_r)  lv_obj_set_pos(leg_r,  50, 113);
    if (foot_l) lv_obj_set_pos(foot_l, 13, 133);
    if (foot_r) lv_obj_set_pos(foot_r, 46, 133);
}

// 全脸重置
static void reset_face(void)
{
    reset_eyes_round();
    reset_mouth_flat();
    reset_brows();
    reset_arms();
    reset_legs();
    if (face_screen) lv_obj_set_style_border_opa(face_screen, LV_OPA_COVER, 0);
}

// ═══════════════════════════════════════════════════════════
//  UI 构建 — Otto 小机器人
// ═══════════════════════════════════════════════════════════

// 创建一个机器人身体部件（统一样式）
static lv_obj_t *make_part(lv_obj_t *parent, int w, int h, int radius,
                             lv_color_t bg, int border_w, lv_color_t border_col)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, bg, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, border_w, 0);
    lv_obj_set_style_border_color(o, border_col, 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static void build_robot_ui(void)
{
    // ══════════════════════════════════════════════════════════
    //  整体布局：所有部件挂在 robot_root 透明容器下
    //  robot_root 居中于 face_area，动画只需移动 robot_root
    //
    //  face_area = 240×132
    //  机器人总高 = 天线球(7) + 天线杆(14) + 头(52) + 颈(6) + 身(34) + 腿(20) + 脚(8) = 141
    //  → robot_root 高 141，顶部对齐 face_area 顶部 -4（略微上移，脚在底边）
    //
    //  robot_root 内坐标（左上角为原点）：
    //  robot_root 宽 = 手臂(10) + 间隙(2) + 身(58) + 间隙(2) + 手臂(10) = 82
    //  cx_local = 41（robot_root 内水平中心）
    // ══════════════════════════════════════════════════════════

    // ── robot_root：透明容器 ─────────────────────────────────
    const int root_w = 82, root_h = 141;
    robot_root = lv_obj_create(face_area);
    lv_obj_set_size(robot_root, root_w, root_h);
    lv_obj_set_style_bg_opa(robot_root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(robot_root, 0, 0);
    lv_obj_set_style_pad_all(robot_root, 0, 0);
    lv_obj_set_style_radius(robot_root, 0, 0);
    lv_obj_clear_flag(robot_root, LV_OBJ_FLAG_SCROLLABLE);
    // 居中于 face_area
    lv_obj_align(robot_root, LV_ALIGN_TOP_MID, 0, -4);

    // 在 robot_root 内的局部坐标
    const int cx = root_w / 2;         // 41

    // 各部件尺寸
    const int head_w = 68, head_h = 52;
    const int body_w = 58, body_h = 34;
    const int arm_w  = 10, arm_h  = 26;
    const int leg_w  = 14, leg_h  = 20;
    const int foot_w = 22, foot_h =  8;
    const int neck_w = 26, neck_h =  6;
    const int ant_w  =  2, ant_h  = 14;
    const int ball_d =  7;

    // Y 坐标（从上往下）
    const int ball_y   = 0;
    const int ant_y    = ball_y + ball_d;
    const int head_y   = ant_y + ant_h;
    const int neck_y   = head_y + head_h;
    const int body_y   = neck_y + neck_h;
    const int leg_y    = body_y + body_h;
    const int foot_y   = leg_y + leg_h;

    // ── 天线（挂在 robot_root） ───────────────────────────────
    antenna_l = make_part(robot_root, ant_w, ant_h, 0, C_ROBOT_EDGE, 0, C_ROBOT_EDGE);
    lv_obj_set_pos(antenna_l, cx - head_w/2 + 14, ant_y);

    antenna_r = make_part(robot_root, ant_w, ant_h, 0, C_ROBOT_EDGE, 0, C_ROBOT_EDGE);
    lv_obj_set_pos(antenna_r, cx + head_w/2 - 16, ant_y);

    antenna_ball_l = make_part(robot_root, ball_d, ball_d, 4, C_CYAN, 0, C_CYAN);
    lv_obj_set_pos(antenna_ball_l, cx - head_w/2 + 11, ball_y);

    antenna_ball_r = make_part(robot_root, ball_d, ball_d, 4, C_CYAN, 0, C_CYAN);
    lv_obj_set_pos(antenna_ball_r, cx + head_w/2 - 18, ball_y);

    // ── 头部（挂在 robot_root） ───────────────────────────────
    robot_head = make_part(robot_root, head_w, head_h, 8, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(robot_head, cx - head_w/2, head_y);

    // ── 颈部（头的子元素，紧贴头底部，填补头→身间隙） ──────
    lv_obj_t *neck = make_part(robot_root, neck_w, neck_h, 2, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(neck, cx - neck_w/2, neck_y);

    // ── 手臂（挂在 robot_root，与躯干同 Y 轴对齐） ──────────
    arm_l = make_part(robot_root, arm_w, arm_h, 4, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(arm_l, cx - body_w/2 - arm_w - 2, body_y + 4);

    arm_r = make_part(robot_root, arm_w, arm_h, 4, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(arm_r, cx + body_w/2 + 2, body_y + 4);

    // 手部关节圆（手臂子元素）
    lv_obj_t *hand_l = make_part(arm_l, 9, 9, 5, C_ROBOT_JOINT, 1, C_ROBOT_EDGE);
    lv_obj_align(hand_l, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_t *hand_r = make_part(arm_r, 9, 9, 5, C_ROBOT_JOINT, 1, C_ROBOT_EDGE);
    lv_obj_align(hand_r, LV_ALIGN_BOTTOM_MID, 0, 0);

    // ── 躯干（挂在 robot_root） ───────────────────────────────
    robot_body = make_part(robot_root, body_w, body_h, 6, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(robot_body, cx - body_w/2, body_y);

    // 躯干胸口装饰（两个小圆灯）
    lv_obj_t *lamp_l = make_part(robot_body, 5, 5, 3, C_CYAN, 0, C_CYAN);
    lv_obj_set_style_bg_opa(lamp_l, LV_OPA_60, 0);
    lv_obj_align(lamp_l, LV_ALIGN_CENTER, -8, -4);
    lv_obj_t *lamp_r = make_part(robot_body, 5, 5, 3, C_CYAN, 0, C_CYAN);
    lv_obj_set_style_bg_opa(lamp_r, LV_OPA_60, 0);
    lv_obj_align(lamp_r, LV_ALIGN_CENTER, 8, -4);
    // 胸口横线
    lv_obj_t *chest_bar = make_part(robot_body, body_w - 12, 2, 1, C_ROBOT_EDGE, 0, C_ROBOT_EDGE);
    lv_obj_set_style_bg_opa(chest_bar, LV_OPA_30, 0);
    lv_obj_align(chest_bar, LV_ALIGN_CENTER, 0, 6);

    // ── 腿（挂在 robot_root） ────────────────────────────────
    leg_l = make_part(robot_root, leg_w, leg_h, 3, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(leg_l, cx - body_w/2 + 6, leg_y);

    leg_r = make_part(robot_root, leg_w, leg_h, 3, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(leg_r, cx + body_w/2 - 6 - leg_w, leg_y);

    // ── 脚（挂在 robot_root） ────────────────────────────────
    foot_l = make_part(robot_root, foot_w, foot_h, 3, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(foot_l, cx - body_w/2 + 2, foot_y);

    foot_r = make_part(robot_root, foot_w, foot_h, 3, C_ROBOT_BODY, 2, C_ROBOT_EDGE);
    lv_obj_set_pos(foot_r, cx + body_w/2 - 2 - foot_w, foot_y);

    // ── 头部屏幕（face_screen 嵌入 robot_head） ───────────────
    face_screen = make_part(robot_head, 52, 38, 4, C_SCREEN_BG, 2, C_CYAN);
    lv_obj_align(face_screen, LV_ALIGN_CENTER, 0, 2);

    // 扫描线（face_screen 子元素，默认隐藏）
    scan_line = make_part(face_screen, 52, 2, 0, C_ORANGE, 0, C_ORANGE);
    lv_obj_set_style_bg_opa(scan_line, LV_OPA_70, 0);
    lv_obj_align(scan_line, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(scan_line, LV_OBJ_FLAG_HIDDEN);

    // ── 眉毛（face_screen 子元素） ───────────────────────────
    brow_l = make_part(face_screen, 10, 2, 1, lv_color_hex(0x00CCFF), 0, C_CYAN);
    lv_obj_align(brow_l, LV_ALIGN_CENTER, -11, -13);

    brow_r = make_part(face_screen, 10, 2, 1, lv_color_hex(0x00CCFF), 0, C_CYAN);
    lv_obj_align(brow_r, LV_ALIGN_CENTER, 11, -13);

    // ── 眼睛（face_screen 子元素） ───────────────────────────
    eye_l = make_part(face_screen, 10, 10, 5, lv_color_hex(0x00CCFF), 0, C_CYAN);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -11, -2);

    pupil_l = make_part(eye_l, 3, 3, 2, C_WHITE, 0, C_WHITE);
    lv_obj_align(pupil_l, LV_ALIGN_TOP_RIGHT, -1, 1);

    eye_r = make_part(face_screen, 10, 10, 5, lv_color_hex(0x00CCFF), 0, C_CYAN);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 11, -2);

    pupil_r = make_part(eye_r, 3, 3, 2, C_WHITE, 0, C_WHITE);
    lv_obj_align(pupil_r, LV_ALIGN_TOP_RIGHT, -1, 1);

    // ── 嘴巴（face_screen 子元素） ───────────────────────────
    mouth_shape = make_part(face_screen, 18, 3, 1, lv_color_hex(0x4488AA), 0, C_CYAN);
    lv_obj_align(mouth_shape, LV_ALIGN_CENTER, 0, 10);

    // ── 装饰物（face_screen 子元素，默认隐藏） ───────────────
    deco_l = make_part(face_screen, 8, 5, 3, C_PINK, 0, C_PINK);
    lv_obj_align(deco_l, LV_ALIGN_CENTER, -20, 4);
    lv_obj_add_flag(deco_l, LV_OBJ_FLAG_HIDDEN);

    deco_r = make_part(face_screen, 8, 5, 3, C_PINK, 0, C_PINK);
    lv_obj_align(deco_r, LV_ALIGN_CENTER, 20, 4);
    lv_obj_add_flag(deco_r, LV_OBJ_FLAG_HIDDEN);

    // ZZZ 标签
    zzz_label = lv_label_create(face_screen);
    lv_obj_set_style_text_font(zzz_label, font_sm, 0);
    lv_obj_set_style_text_color(zzz_label, C_DIM_BLUE, 0);
    lv_label_set_text(zzz_label, "z z");
    lv_obj_align(zzz_label, LV_ALIGN_TOP_RIGHT, -2, 2);
    lv_obj_add_flag(zzz_label, LV_OBJ_FLAG_HIDDEN);

    // ！标签（惊讶）
    exclaim = lv_label_create(face_screen);
    lv_obj_set_style_text_font(exclaim, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(exclaim, C_YELLOW, 0);
    lv_label_set_text(exclaim, "!!");
    lv_obj_align(exclaim, LV_ALIGN_TOP_MID, 0, 1);
    lv_obj_add_flag(exclaim, LV_OBJ_FLAG_HIDDEN);

    // 记录 robot_root 真实 x 坐标（align 后才有效）
    lv_obj_update_layout(face_area);
    s_robot_root_x0 = lv_obj_get_x(robot_root);
}
// ─── 对话区 ───
static lv_obj_t *mouth_area    = NULL;
static lv_obj_t *status_label  = NULL;
static lv_obj_t *wifi_icon     = NULL;
static lv_obj_t *stream_label  = NULL;

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
//  状态表情动画 — Otto 机器人版（22种，精准动画）
// ═══════════════════════════════════════════════════════════

// ── 睡觉：眼睛闭成细线，ZZZ漂浮，缓慢呼吸光 ──────────────
// 关键动画：超慢呼吸节奏，ZZZ逐渐上浮消散，天线球微光闪烁
static void expr_sleeping(void)
{
    set_screen_glow(C_DIM_BLUE);
    // 眼睛：水平细线（眯眼睡觉）
    lv_obj_set_size(eye_l, 12, 2);
    lv_obj_set_size(eye_r, 12, 2);
    lv_obj_set_style_radius(eye_l, 1, 0);
    lv_obj_set_style_radius(eye_r, 1, 0);
    set_eye_color(lv_color_hex(0x1A3A5A));
    // 嘴巴：放松的小弧形（睡着了）
    lv_obj_set_size(mouth_shape, 14, 4);
    lv_obj_set_style_radius(mouth_shape, 4, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x1A2A3A), 0);
    // ZZZ 显示并上浮
    lv_obj_clear_flag(zzz_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(zzz_label, lv_color_hex(0x2A4A6A), 0);
    make_inf(zzz_label, anim_y_cb, 2, -14, 2500, 500);
    make_inf(zzz_label, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 2500, 500);
    // 屏幕边框呼吸（极慢，模拟睡眠节奏）
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_10, LV_OPA_50, 3000, 3000);
    // 天线球微弱脉动
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_20, LV_OPA_60, 3000, 3000);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_20, LV_OPA_60, 3000, 3000);
    // 整体机器人轻微下沉（放松姿态）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0, ROBOT_ROOT_Y0+2, 3000, 3000);
}

// ── 联网中：橙色扫描线扫过屏幕，眼睛左右搜索 ──────────────
// 关键动画：扫描线循环扫过，眼睛左右来回，天线球快速闪烁
static void expr_connecting(void)
{
    set_screen_glow(C_ORANGE);
    // 眼睛：圆眼，橙色（正在搜索）
    lv_obj_set_size(eye_l, 10, 10);
    lv_obj_set_size(eye_r, 10, 10);
    set_eye_color(C_ORANGE);
    // 嘴巴：中性直线（专注状态）
    lv_obj_set_size(mouth_shape, 14, 3);
    // 扫描线：橙色从上至下循环
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(scan_line, C_ORANGE, 0);
    make_inf(scan_line, anim_y_cb, 0, 36, 600, 100);
    // 眼睛左右来回扫（搜索信号感）
    make_inf(eye_l, anim_x_cb,
             EYE_L_X0-8, EYE_L_X0+8, 700, 700);
    make_inf(eye_r, anim_x_cb,
             EYE_R_X0-8, EYE_R_X0+8, 700, 700);
    // 天线球快速闪烁（发射信号感）
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_20, LV_OPA_COVER, 300, 300);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_COVER, LV_OPA_20, 300, 300);
}

// ── 已连接：青色，眼睛向上弹跳庆祝，嘴角上扬 ─────────────
// 关键动画：单次眼睛弹起+回落，屏幕边框稳定脉动
static void expr_connected(void)
{
    set_screen_glow(C_CYAN);
    // 眼睛：青色，正常大小
    lv_obj_set_size(eye_l, 10, 10);
    lv_obj_set_size(eye_r, 10, 10);
    set_eye_color(C_CYAN);
    // 嘴巴：宽宽的满意微笑
    lv_obj_set_size(mouth_shape, 20, 5);
    lv_obj_set_style_radius(mouth_shape, 3, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x004444), 0);
    // 眼睛弹起庆祝（单次）
    make_once(eye_l, anim_h_cb, 10, 14, 120, 120);
    make_once(eye_r, anim_h_cb, 10, 14, 120, 120);
    // 整体头部小幅弹起
    make_once(robot_root, anim_y_cb,
              ROBOT_ROOT_Y0, ROBOT_ROOT_Y0-5, 150, 200);
    // 屏幕边框平稳脉动
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_60, LV_OPA_COVER, 800, 800);
}

// ── 网络错误：红色，眉头紧锁，嘴角下垂，头部剧烈抖动 ──────
// 关键动画：高频抖动（焦虑感），眉毛下压（愤怒/担忧），嘴角下撇
static void expr_error(void)
{
    set_screen_glow(C_RED);
    // 眼睛：红色，扁平（眉头紧锁的眼神）
    lv_obj_set_size(eye_l, 10, 6);
    lv_obj_set_size(eye_r, 10, 6);
    lv_obj_set_style_radius(eye_l, 2, 0);
    lv_obj_set_style_radius(eye_r, 2, 0);
    set_eye_color(C_RED);
    // 眉毛：大幅内压（倒八字，愤怒/担忧）
    lv_obj_align(brow_l, LV_ALIGN_CENTER, -8, -13);
    lv_obj_align(brow_r, LV_ALIGN_CENTER, 8, -13);
    lv_obj_set_style_bg_color(brow_l, C_RED, 0);
    lv_obj_set_style_bg_color(brow_r, C_RED, 0);
    // 嘴巴：下撇皱嘴（不爽/担忧）
    lv_obj_set_size(mouth_shape, 14, 3);
    lv_obj_set_style_bg_color(mouth_shape, C_RED, 0);
    lv_obj_align(mouth_shape, LV_ALIGN_CENTER, 0, 12);
    // 高频左右抖动（错误感/焦虑）
    make_inf(face_screen, anim_x_cb, 4, 12, 70, 70);
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_50, LV_OPA_COVER, 150, 150);
}

// ── 思考中：紫色，眼睛斜向右上（凝神），天线闪烁（运算中） ─
// 关键动画：天线球规律闪烁模拟CPU运算，眼睛固定看向一侧（思考方向）
static void expr_thinking(void)
{
    set_screen_glow(C_PURPLE);
    // 眼睛：紫色，偏向右上（凝视空中思考）
    lv_obj_set_size(eye_l, 8, 8);
    lv_obj_set_size(eye_r, 8, 8);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -6, -8);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 12, -8);
    set_eye_color(C_PURPLE);
    // 左眉轻微下压（单侧皱眉，思考神情）
    lv_obj_align(brow_l, LV_ALIGN_CENTER, -14, -14);
    lv_obj_set_style_bg_color(brow_l, C_PURPLE, 0);
    lv_obj_set_style_bg_color(brow_r, C_PURPLE, 0);
    // 嘴巴：歪嘴（思考中不确定的表情）
    lv_obj_set_size(mouth_shape, 10, 3);
    lv_obj_align(mouth_shape, LV_ALIGN_CENTER, 4, 10);
    // 屏幕边框脉动（大脑运转感）
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_40, LV_OPA_COVER, 600, 600);
    // 天线球交替闪烁（模拟AI运算信号）
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_COVER, LV_OPA_20, 400, 200);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_20, LV_OPA_COVER, 400, 200);
}

// ── 在码字：青色，眼睛快速左右扫（看屏幕），手臂上下抖（打字）
// 关键动画：眼睛快速来回扫，手臂高频振动（模拟快速打字）
static void expr_speaking(void)
{
    set_screen_glow(C_CYAN);
    // 眼睛：聚焦，看向中间（专注打字）
    lv_obj_set_size(eye_l, 10, 10);
    lv_obj_set_size(eye_r, 10, 10);
    set_eye_color(lv_color_hex(0x008888));
    // 嘴巴：咧开（专注工作中带着一点得意）
    lv_obj_set_size(mouth_shape, 20, 5);
    lv_obj_set_style_radius(mouth_shape, 3, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x004433), 0);
    // 眼睛高频左右扫（快速阅读/编写内容）
    make_inf(eye_l, anim_x_cb,
             EYE_L_X0-10, EYE_L_X0+10, 180, 30);
    make_inf(eye_r, anim_x_cb,
             EYE_R_X0-10, EYE_R_X0+10, 180, 30);
    // 手臂交替抖动（打字/写作动作感）
    make_inf(arm_l, anim_y_cb,
             ARM_Y0-4, ARM_Y0+4, 120, 120);
    make_inf(arm_r, anim_y_cb,
             ARM_Y0+4, ARM_Y0-4, 120, 120);
}

// ── 在听：绿色，大睁眼，嘴微张O形，头部前倾点头 ───────────
// 关键动画：头部轻轻前后点头（认真聆听），眼睛大而专注
static void expr_listening(void)
{
    set_screen_glow(C_GREEN);
    // 眼睛：绿色，大圆眼（专注聆听，瞪大眼睛）
    lv_obj_set_size(eye_l, 12, 12);
    lv_obj_set_size(eye_r, 12, 12);
    lv_obj_set_style_radius(eye_l, 6, 0);
    lv_obj_set_style_radius(eye_r, 6, 0);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -12, -3);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 12, -3);
    set_eye_color(C_GREEN);
    // 嘴巴：微张O形（在听时自然张嘴）
    lv_obj_set_size(mouth_shape, 10, 7);
    lv_obj_set_style_radius(mouth_shape, 5, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x003300), 0);
    // 头部前后轻点（倾听点头，有节奏感）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0-3, ROBOT_ROOT_Y0+3, 600, 600);
    // 天线球柔和脉动（接收信号）
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_60, LV_OPA_COVER, 600, 600);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_60, LV_OPA_COVER, 600, 600);
}

// ── 开心：黄色，弯月眼（笑眼），大咧嘴，整体上下蹦跳 ──────
// 关键动画：整体弹跳（喜悦跳动），装饰闪烁，天线球欢快跳动
static void expr_happy(void)
{
    set_screen_glow(C_YELLOW);
    // 眼睛：弯月形（笑眼，半圆），黄色
    lv_obj_set_size(eye_l, 12, 6);
    lv_obj_set_size(eye_r, 12, 6);
    lv_obj_set_style_radius(eye_l, 6, 0);
    lv_obj_set_style_radius(eye_r, 6, 0);
    set_eye_color(C_YELLOW);
    // 嘴巴：大宽咧嘴（哈哈大笑）
    lv_obj_set_size(mouth_shape, 24, 8);
    lv_obj_set_style_radius(mouth_shape, 5, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x4A3000), 0);
    // 装饰（星光）出现
    lv_obj_set_style_bg_color(deco_l, C_YELLOW, 0);
    lv_obj_set_style_bg_color(deco_r, C_YELLOW, 0);
    lv_obj_set_style_radius(deco_l, 1, 0);
    lv_obj_set_style_radius(deco_r, 1, 0);
    lv_obj_clear_flag(deco_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(deco_r, LV_OBJ_FLAG_HIDDEN);
    make_inf(deco_l, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 300, 300);
    make_inf(deco_r, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 450, 450);
    // 机器人整体快乐蹦跳（头+手臂同步跳）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0-6, ROBOT_ROOT_Y0+2, 350, 350);
    make_inf(arm_l, anim_y_cb,
             ARM_Y0-5, ARM_Y0+2, 350, 350);
    make_inf(arm_r, anim_y_cb,
             ARM_Y0-5, ARM_Y0+2, 350, 350);
    // 天线球欢快闪烁
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_40, LV_OPA_COVER, 200, 200);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_COVER, LV_OPA_40, 200, 200);
}

// ── 神游：蓝白色，眼神飘远（看向右上空），整体缓慢漂移 ─────
// 关键动画：机器人极慢左右漂移（心不在焉），眼神空洞偏向一侧
static void expr_daydream(void)
{
    set_screen_glow(lv_color_hex(0x6088CC));
    // 屏幕半透明（神游感，有些恍惚）
    lv_obj_set_style_bg_opa(face_screen, LV_OPA_70, 0);
    // 眼睛：小而偏右上（目光游离到远方）
    lv_obj_set_size(eye_l, 7, 7);
    lv_obj_set_size(eye_r, 7, 7);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -4, -8);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 14, -8);
    set_eye_color(lv_color_hex(0x2244AA));
    // 嘴巴：放松微微上扬（愉快的神游）
    lv_obj_set_size(mouth_shape, 12, 4);
    lv_obj_set_style_radius(mouth_shape, 3, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x1A2A5A), 0);
    // 整体极慢漂移（心不在焉，飘飘然）
    make_inf(face_screen, anim_x_cb, 0, 16, 4000, 4000);
    // 屏幕边框缓慢呼吸（恍惚感）
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_20, LV_OPA_60, 2500, 2500);
}

// ── 贪恋爱：粉色，爱心眼（方形模拟），心形浮上，心跳节奏跳 ─
// 关键动画：心跳节奏弹动（砰砰感），爱心装饰上浮
static void expr_in_love(void)
{
    set_screen_glow(C_PINK);
    // 眼睛：粉色，方形（模拟爱心形状）
    lv_obj_set_size(eye_l, 10, 10);
    lv_obj_set_size(eye_r, 10, 10);
    lv_obj_set_style_radius(eye_l, 2, 0);
    lv_obj_set_style_radius(eye_r, 2, 0);
    set_eye_color(C_PINK);
    // 嘴巴：O形（被萌到张嘴）
    lv_obj_set_size(mouth_shape, 10, 10);
    lv_obj_set_style_radius(mouth_shape, 5, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x4A0A2A), 0);
    // 装饰（粉色心形）向上浮动
    lv_obj_set_style_bg_color(deco_l, C_PINK, 0);
    lv_obj_set_style_bg_color(deco_r, C_PINK, 0);
    lv_obj_set_style_radius(deco_l, 5, 0);
    lv_obj_set_style_radius(deco_r, 5, 0);
    lv_obj_clear_flag(deco_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(deco_r, LV_OBJ_FLAG_HIDDEN);
    make_inf(deco_l, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 800, 200);
    make_inf(deco_l, anim_y_cb, 4, -12, 800, 200);
    make_inf(deco_r, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 1000, 200);
    make_inf(deco_r, anim_y_cb, 4, -12, 1000, 200);
    // 心跳节奏：快-慢（砰砰~）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0-4, ROBOT_ROOT_Y0+2, 300, 600);
    // 天线球粉色呼吸
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_50, LV_OPA_COVER, 600, 600);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_50, LV_OPA_COVER, 600, 600);
}

// ── 吃饭：橙色，眯眼（享受），嘴巴咀嚼上下动，点头感 ───────
// 关键动画：嘴巴高度来回变（咀嚼），头轻微上下（吃饭点头）
static void expr_eating(void)
{
    set_screen_glow(C_ORANGE);
    // 眼睛：弯眯眼（享受美食闭眼感）
    lv_obj_set_size(eye_l, 12, 5);
    lv_obj_set_size(eye_r, 12, 5);
    lv_obj_set_style_radius(eye_l, 5, 0);
    lv_obj_set_style_radius(eye_r, 5, 0);
    set_eye_color(lv_color_hex(0x4A2800));
    // 嘴巴：中等大小，咀嚼用
    lv_obj_set_size(mouth_shape, 16, 5);
    lv_obj_set_style_radius(mouth_shape, 3, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x5A2800), 0);
    // 嘴巴咀嚼动画（高度交替变化）
    make_inf(mouth_shape, anim_h_cb, 3, 9, 250, 250);
    // 头部轻微点头（吃得津津有味）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0-1, ROBOT_ROOT_Y0+3, 350, 350);
}

// ── 健身：荧光绿，眯眼用力，手臂高举，机器人抖动 ─────────
// 关键动画：手臂举起到最高，整体高频震动（用力感），汗水装饰
static void expr_workout(void)
{
    set_screen_glow(C_LIME);
    // 眼睛：绿色细条（用力眯眼）
    lv_obj_set_size(eye_l, 10, 4);
    lv_obj_set_size(eye_r, 10, 4);
    lv_obj_set_style_radius(eye_l, 2, 0);
    lv_obj_set_style_radius(eye_r, 2, 0);
    set_eye_color(C_LIME);
    // 眉毛：大幅下压（用力/咬牙切齿）
    lv_obj_align(brow_l, LV_ALIGN_CENTER, -14, -12);
    lv_obj_align(brow_r, LV_ALIGN_CENTER, 14, -12);
    lv_obj_set_style_bg_color(brow_l, C_LIME, 0);
    lv_obj_set_style_bg_color(brow_r, C_LIME, 0);
    // 嘴巴：咬牙（宽矩形）
    lv_obj_set_size(mouth_shape, 18, 4);
    lv_obj_set_style_radius(mouth_shape, 0, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x1A5A00), 0);
    // 手臂高举（举重姿态）
    // 手臂高举（举重姿态：y上移约14px）
    lv_obj_set_pos(arm_l, 0,  69);
    lv_obj_set_pos(arm_r, 72, 69);
    // 整体高频震动（用力感）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0-2, ROBOT_ROOT_Y0+2, 90, 90);
    // 汗水装饰下滑
    lv_obj_set_style_bg_color(deco_r, lv_color_hex(0x44AAFF), 0);
    lv_obj_set_style_radius(deco_r, 4, 0);
    lv_obj_set_size(deco_r, 5, 8);
    lv_obj_align(deco_r, LV_ALIGN_TOP_RIGHT, -2, 4);
    lv_obj_clear_flag(deco_r, LV_OBJ_FLAG_HIDDEN);
    make_inf(deco_r, anim_y_cb, 4, 28, 800, 100);
    make_inf(deco_r, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 800, 100);
}

// ── 学习：蓝色，眼睛缓慢左右扫（读书），专注眉，稳定脉动 ──
// 关键动画：眼睛平缓来回扫视（模拟阅读行文），屏幕稳定蓝光
static void expr_studying(void)
{
    set_screen_glow(C_BLUE);
    // 眼睛：蓝色，中等大小（认真阅读）
    lv_obj_set_size(eye_l, 10, 9);
    lv_obj_set_size(eye_r, 10, 9);
    lv_obj_set_style_radius(eye_l, 4, 0);
    lv_obj_set_style_radius(eye_r, 4, 0);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -12, -5);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 12, -5);
    set_eye_color(C_BLUE);
    // 眉毛：轻微下压（专注皱眉）
    lv_obj_align(brow_l, LV_ALIGN_CENTER, -14, -14);
    lv_obj_align(brow_r, LV_ALIGN_CENTER, 14, -14);
    lv_obj_set_style_bg_color(brow_l, C_BLUE, 0);
    lv_obj_set_style_bg_color(brow_r, C_BLUE, 0);
    // 嘴巴：紧闭直线（专注无暇说话）
    lv_obj_set_size(mouth_shape, 12, 2);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x224488), 0);
    // 眼睛缓慢左右扫（逐行阅读感）
    make_inf(eye_l, anim_x_cb,
             EYE_L_X0-6, EYE_L_X0+6, 1800, 200);
    make_inf(eye_r, anim_x_cb,
             EYE_R_X0-6, EYE_R_X0+6, 1800, 200);
    // 屏幕稳定蓝光脉动（沉浸学习感）
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_50, LV_OPA_COVER, 1200, 1200);
}

// ── 看电视：白色，超大圆眼（目瞪口呆），嘴巴张开，眼睛跟着跳
// 关键动画：眼睛偶尔快速左右跳动（跟随画面），屏幕反光闪烁
static void expr_watching_tv(void)
{
    set_screen_glow(C_WHITE);
    // 眼睛：超大圆眼（被画面吸引，目不转睛）
    lv_obj_set_size(eye_l, 14, 14);
    lv_obj_set_size(eye_r, 14, 14);
    lv_obj_set_style_radius(eye_l, 7, 0);
    lv_obj_set_style_radius(eye_r, 7, 0);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -12, -2);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 12, -2);
    set_eye_color(lv_color_hex(0xCCEEFF));
    // 嘴巴：张开O形（看到好东西目瞪口呆）
    lv_obj_set_size(mouth_shape, 12, 10);
    lv_obj_set_style_radius(mouth_shape, 5, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x1A1A2A), 0);
    // 眼睛偶尔突然左右跳（跟随画面切换）
    make_inf(eye_l, anim_x_cb,
             EYE_L_X0-10, EYE_L_X0+10, 1400, 50);
    make_inf(eye_r, anim_x_cb,
             EYE_R_X0-10, EYE_R_X0+10, 1400, 50);
    // 屏幕反光闪烁（电视画面投影）
    make_inf(face_screen, anim_opa_cb, LV_OPA_80, LV_OPA_COVER, 1800, 80);
}

// ── 不想理你：灰色，眼睛斜向一侧，嘴角下撇，极慢转头无视 ──
// 关键动画：整体极慢漂移+偶尔回头（爱理不理），眼神斜视
static void expr_ignoring(void)
{
    set_screen_glow(C_GRAY);
    // 眼睛：灰色，半眯斜视（斜眼嫌弃）
    lv_obj_set_size(eye_l, 8, 5);
    lv_obj_set_size(eye_r, 8, 5);
    lv_obj_set_style_radius(eye_l, 2, 0);
    lv_obj_set_style_radius(eye_r, 2, 0);
    // 眼睛整体偏右侧（不屑看你，眼神瞟向别处）
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -4, -2);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 14, -2);
    set_eye_color(C_GRAY);
    // 嘴巴：小嘟嘴（不爽/不屑）
    lv_obj_set_size(mouth_shape, 10, 5);
    lv_obj_set_style_radius(mouth_shape, 4, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x404040), 0);
    // 整体极慢漂移（懒洋洋地不理人）
    make_inf(face_screen, anim_x_cb, 0, 16, 5000, 5000);
    // 屏幕暗淡呼吸（没精打采）
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_20, LV_OPA_50, 2500, 2500);
}

// ── 生气：红色，眉毛倒八字下压，扁眼，高频剧烈抖动 ─────────
// 关键动画：极高频抖动（愤怒颤抖），眉毛倒八字，嘴咬牙直线
static void expr_angry(void)
{
    set_screen_glow(C_RED);
    // 眼睛：红色，扁条（怒眼）
    lv_obj_set_size(eye_l, 10, 5);
    lv_obj_set_size(eye_r, 10, 5);
    lv_obj_set_style_radius(eye_l, 1, 0);
    lv_obj_set_style_radius(eye_r, 1, 0);
    set_eye_color(C_RED);
    // 眉毛：倒八字（内侧大幅压低，极度愤怒）
    lv_obj_set_size(brow_l, 12, 3);
    lv_obj_set_size(brow_r, 12, 3);
    lv_obj_align(brow_l, LV_ALIGN_CENTER, -10, -12);
    lv_obj_align(brow_r, LV_ALIGN_CENTER, 10, -12);
    lv_obj_set_style_bg_color(brow_l, C_RED, 0);
    lv_obj_set_style_bg_color(brow_r, C_RED, 0);
    // 嘴巴：咬紧的直线（怒咬牙关）
    lv_obj_set_size(mouth_shape, 20, 3);
    lv_obj_set_style_radius(mouth_shape, 0, 0);
    lv_obj_set_style_bg_color(mouth_shape, C_RED, 0);
    // 机器人高频剧烈抖动（愤怒颤抖）
    make_inf(face_screen, anim_x_cb, 3, 13, 55, 55);
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_60, LV_OPA_COVER, 100, 100);
    // 手臂颤动（愤怒握拳感）
    make_inf(arm_l, anim_y_cb,
             ARM_Y0-2, ARM_Y0+2, 55, 55);
    make_inf(arm_r, anim_y_cb,
             ARM_Y0-2, ARM_Y0+2, 55, 55);
}

// ── 惊讶：黄色边框，超大圆眼，大嘴O形，弹起+叹号 ─────────
// 关键动画：单次强力弹起（吓一跳），叹号出现，眼睛超大
static void expr_surprised(void)
{
    set_screen_glow(C_YELLOW);
    // 眼睛：白色超大（瞪大吓到），圆眼
    lv_obj_set_size(eye_l, 14, 14);
    lv_obj_set_size(eye_r, 14, 14);
    lv_obj_set_style_radius(eye_l, 7, 0);
    lv_obj_set_style_radius(eye_r, 7, 0);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -12, -3);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 12, -3);
    set_eye_color(lv_color_hex(0xFFFFFF));
    // 嘴巴：大O（张嘴惊呼）
    lv_obj_set_size(mouth_shape, 14, 14);
    lv_obj_set_style_radius(mouth_shape, 7, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x2A1000), 0);
    // 叹号出现
    lv_obj_clear_flag(exclaim, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(exclaim, C_YELLOW, 0);
    // 机器人弹起（吓一跳）
    make_once(robot_root, anim_y_cb,
              ROBOT_ROOT_Y0, ROBOT_ROOT_Y0-12, 100, 250);
    // 后续持续高速跳动（心还在跳）
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_70, LV_OPA_COVER, 200, 200);
    // 天线球疯狂闪烁（被吓到了）
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 150, 150);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 150, 150);
}

// ── 无聊：暗灰，眼皮沉重，嘴角下撇，极慢摇晃（昏昏欲睡） ──
// 关键动画：极慢摇晃（精神萎靡），眼睛沉重快合上
static void expr_bored(void)
{
    set_screen_glow(lv_color_hex(0x303040));
    // 眼睛：暗灰，细条（快睁不开了）
    lv_obj_set_size(eye_l, 10, 3);
    lv_obj_set_size(eye_r, 10, 3);
    lv_obj_set_style_radius(eye_l, 1, 0);
    lv_obj_set_style_radius(eye_r, 1, 0);
    set_eye_color(lv_color_hex(0x505050));
    // 嘴巴：下撇（嘟嘴无聊感）
    lv_obj_set_size(mouth_shape, 16, 3);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x2A2A35), 0);
    lv_obj_align(mouth_shape, LV_ALIGN_CENTER, 0, 11);
    // 极慢上下摇晃（精神萎靡，脑袋快撑不住了）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0-1, ROBOT_ROOT_Y0+4, 5000, 5000);
    // 屏幕极暗（兴趣全无）
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_20, LV_OPA_40, 3000, 3000);
    // 手臂下垂感
    make_inf(arm_l, anim_y_cb,
             ARM_Y0, ARM_Y0+3, 5000, 5000);
    make_inf(arm_r, anim_y_cb,
             ARM_Y0, ARM_Y0+3, 5000, 5000);
}

// ── 赛博模式：荧光绿，方形眼，高速扫描线，屏幕快闪 ─────────
// 关键动画：极高速绿色扫描，眼睛方形闪烁，赛博感十足
static void expr_cyber(void)
{
    set_screen_glow(C_LIME);
    // 屏幕背景变为深绿（赛博感）
    lv_obj_set_style_bg_color(face_screen, lv_color_hex(0x000800), 0);
    // 眼睛：荧光绿，方形（赛博机器人）
    lv_obj_set_size(eye_l, 12, 6);
    lv_obj_set_size(eye_r, 12, 6);
    lv_obj_set_style_radius(eye_l, 0, 0);
    lv_obj_set_style_radius(eye_r, 0, 0);
    set_eye_color(C_LIME);
    // 嘴巴：矩形绿条（机器人显示器风格）
    lv_obj_set_size(mouth_shape, 18, 4);
    lv_obj_set_style_radius(mouth_shape, 0, 0);
    lv_obj_set_style_bg_color(mouth_shape, C_LIME, 0);
    // 高速绿色扫描线（数据扫描感）
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(scan_line, C_LIME, 0);
    lv_obj_set_style_bg_opa(scan_line, LV_OPA_80, 0);
    make_inf(scan_line, anim_y_cb, 0, 38, 120, 30);
    // 眼睛高频闪烁（数据处理中）
    make_inf(eye_l, anim_opa_cb, LV_OPA_30, LV_OPA_COVER, 80, 80);
    make_inf(eye_r, anim_opa_cb, LV_OPA_COVER, LV_OPA_30, 80, 80);
    // 头部快速震动（超级计算）
    make_inf(face_screen, anim_x_cb, 7, 9, 60, 60);
}

// ── 发晕：黄色，眼睛脉动缩放（模拟螺旋），整体左右大幅晃 ───
// 关键动画：整体大幅摇晃（站不稳），眼睛螺旋脉动（眼冒金星）
static void expr_dizzy(void)
{
    set_screen_glow(C_YELLOW);
    // 眼睛：黄色，方形（发晕时眼睛失焦）
    lv_obj_set_size(eye_l, 10, 10);
    lv_obj_set_size(eye_r, 10, 10);
    lv_obj_set_style_radius(eye_l, 2, 0);
    lv_obj_set_style_radius(eye_r, 2, 0);
    set_eye_color(C_YELLOW);
    // 眼睛脉动缩放（模拟眼冒金星）
    make_inf(eye_l, anim_w_cb, 4, 10, 250, 250);
    make_inf(eye_r, anim_w_cb, 10, 4, 250, 250);
    make_inf(eye_l, anim_h_cb, 4, 10, 350, 350);
    make_inf(eye_r, anim_h_cb, 10, 4, 350, 350);
    // 嘴巴：弯曲（发晕，嘴歪）
    lv_obj_set_size(mouth_shape, 14, 5);
    lv_obj_set_style_radius(mouth_shape, 4, 0);
    lv_obj_align(mouth_shape, LV_ALIGN_CENTER, -2, 11);
    // 整体大幅左右摇晃（站不稳）
    make_inf(face_screen, anim_x_cb, -2, 18, 220, 220);
    // 手臂跟着晃（站不稳）
    make_inf(arm_l, anim_y_cb,
             ARM_Y0-3, ARM_Y0+3, 220, 220);
    make_inf(arm_r, anim_y_cb,
             ARM_Y0+3, ARM_Y0-3, 220, 220);
}

// ── 害羞：粉色，眼睛往下看，腮红出现，头轻轻往下低 ─────────
// 关键动画：腮红脉动出现，汗珠闪现，头部微微前倾
static void expr_shy(void)
{
    set_screen_glow(C_PINK);
    // 眼睛：粉色，往下看（害羞不敢看人）
    lv_obj_set_size(eye_l, 8, 6);
    lv_obj_set_size(eye_r, 8, 6);
    lv_obj_set_style_radius(eye_l, 3, 0);
    lv_obj_set_style_radius(eye_r, 3, 0);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -12, 2);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 12, 2);
    set_eye_color(lv_color_hex(0x662244));
    // 嘴巴：小弧形微笑（羞涩的笑）
    lv_obj_set_size(mouth_shape, 10, 5);
    lv_obj_set_style_radius(mouth_shape, 4, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x4A1030), 0);
    // 腮红（害羞脸红）
    lv_obj_set_style_bg_color(deco_l, lv_color_hex(0xFF6688), 0);
    lv_obj_set_style_bg_color(deco_r, lv_color_hex(0xFF6688), 0);
    lv_obj_set_style_radius(deco_l, 4, 0);
    lv_obj_set_style_radius(deco_r, 4, 0);
    lv_obj_set_size(deco_l, 10, 6);
    lv_obj_set_size(deco_r, 10, 6);
    lv_obj_align(deco_l, LV_ALIGN_CENTER, -18, 6);
    lv_obj_align(deco_r, LV_ALIGN_CENTER, 18, 6);
    lv_obj_clear_flag(deco_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(deco_r, LV_OBJ_FLAG_HIDDEN);
    make_inf(deco_l, anim_opa_cb, LV_OPA_30, LV_OPA_70, 1000, 1000);
    make_inf(deco_r, anim_opa_cb, LV_OPA_30, LV_OPA_70, 1000, 1000);
    // 头部微微低下（害羞低头）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0, ROBOT_ROOT_Y0+3, 1500, 1500);
    // 天线球粉色微光
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_40, LV_OPA_COVER, 1200, 1200);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_40, LV_OPA_COVER, 1200, 1200);
}

// ── 亢奋：金色，超大圆眼，大嘴咧嘴，全体高频大幅跳动 ───────
// 关键动画：整体高频大幅弹跳（压抑不住的兴奋），天线疯狂闪
static void expr_excited(void)
{
    set_screen_glow(C_GOLD);
    // 眼睛：金色，超大圆眼（激动得眼睛发光）
    lv_obj_set_size(eye_l, 13, 13);
    lv_obj_set_size(eye_r, 13, 13);
    lv_obj_set_style_radius(eye_l, 7, 0);
    lv_obj_set_style_radius(eye_r, 7, 0);
    lv_obj_align(eye_l, LV_ALIGN_CENTER, -12, -3);
    lv_obj_align(eye_r, LV_ALIGN_CENTER, 12, -3);
    set_eye_color(C_GOLD);
    // 嘴巴：咧嘴大笑到最宽
    lv_obj_set_size(mouth_shape, 26, 9);
    lv_obj_set_style_radius(mouth_shape, 5, 0);
    lv_obj_set_style_bg_color(mouth_shape, lv_color_hex(0x4A3800), 0);
    // 眼睛超快闪烁（激动得眼睛都在抖）
    make_inf(eye_l, anim_opa_cb, LV_OPA_60, LV_OPA_COVER, 120, 120);
    make_inf(eye_r, anim_opa_cb, LV_OPA_COVER, LV_OPA_60, 120, 120);
    // 整体高频大幅弹跳（真的压制不住）
    make_inf(robot_root, anim_y_cb,
             ROBOT_ROOT_Y0-8, ROBOT_ROOT_Y0+4, 180, 180);
    make_inf(arm_l, anim_y_cb,
             ARM_Y0-6, ARM_Y0+6, 160, 160);
    make_inf(arm_r, anim_y_cb,
             ARM_Y0+6, ARM_Y0-6, 160, 160);
    // 天线球疯狂闪烁（发射兴奋信号）
    make_inf(antenna_ball_l, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 100, 100);
    make_inf(antenna_ball_r, anim_opa_cb, LV_OPA_COVER, LV_OPA_0, 100, 100);
}

// ── 配置模式：青色，扫描线扫过，等待中 ────────────────────
static void expr_config(void)
{
    set_screen_glow(C_CYAN);
    lv_obj_set_size(eye_l, 8, 8);
    lv_obj_set_size(eye_r, 8, 8);
    set_eye_color(lv_color_hex(0x006688));
    lv_obj_set_size(mouth_shape, 12, 3);
    lv_obj_clear_flag(scan_line, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(scan_line, C_CYAN, 0);
    make_inf(scan_line, anim_y_cb, 0, 70, 1200, 300);
    make_inf(face_screen, anim_border_opa_cb, LV_OPA_40, LV_OPA_100, 800, 800);
}

// ═══════════════════════════════════════════════════════════
//  状态切换主函数
// ═══════════════════════════════════════════════════════════

static void update_state_display(lcd_state_t state)
{
    stop_all_anims();
    hide_all_decorations();
    reset_face();

    // 复位 robot_root 到初始位置（防止动画残留漂移）
    if (robot_root) lv_obj_align(robot_root, LV_ALIGN_TOP_MID, 0, ROBOT_ROOT_Y0);
    // 复位手臂
    if (arm_l) lv_obj_set_pos(arm_l, 0,  ARM_Y0);
    if (arm_r) lv_obj_set_pos(arm_r, 72, ARM_Y0);

    // 恢复屏幕背景色
    lv_obj_set_style_bg_color(face_screen, C_SCREEN_BG, 0);
    lv_obj_set_style_bg_opa(face_screen, LV_OPA_COVER, 0);

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

static bool is_transient_state(lcd_state_t state)
{
    return state == LCD_STATE_CONNECTING ||
           state == LCD_STATE_CONNECTED ||
           state == LCD_STATE_ERROR ||
           state == LCD_STATE_THINKING ||
           state == LCD_STATE_SPEAKING ||
           state == LCD_STATE_LISTENING ||
           state == LCD_STATE_CONFIG;
}

static bool is_valid_base_mood(lcd_state_t state)
{
    return state > LCD_STATE_LISTENING &&
           state < LCD_STATE_CONFIG;
}

static void apply_state_locked(lcd_state_t state)
{
    current_state = state;
    update_state_display(state);
}

static const lcd_state_t mood_pool[] = {
    LCD_STATE_SLEEPING,
    LCD_STATE_HAPPY,
    LCD_STATE_DAYDREAM,
    LCD_STATE_IN_LOVE,
    LCD_STATE_EATING,
    LCD_STATE_STUDYING,
    LCD_STATE_WATCHING_TV,
    LCD_STATE_ANGRY,
    LCD_STATE_SURPRISED,
    LCD_STATE_BORED,
    LCD_STATE_DIZZY,
    LCD_STATE_SHY,
    LCD_STATE_EXCITED,
};
#define MOOD_POOL_SIZE (int)(sizeof(mood_pool)/sizeof(mood_pool[0]))

static void mood_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (is_transient_state(current_state)) return;

    mood_ticks++;
    if (mood_ticks < 30) return;
    mood_ticks = 0;

    uint32_t r = (uint32_t)(esp_timer_get_time() & 0xFFFF);
    if ((r % 10) >= 3) return;

    lcd_state_t mood = mood_pool[r % MOOD_POOL_SIZE];

    if (lvgl_port_lock(0)) {
        base_mood = mood;
        apply_state_locked(base_mood);
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
    if (!eye_l || !eye_r) return;
    if (lvgl_port_lock(0)) {
        make_once(eye_l, anim_h_cb, lv_obj_get_height(eye_l), 1, 80, 80);
        make_once(eye_r, anim_h_cb, lv_obj_get_height(eye_r), 1, 80, 80);
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
            lv_obj_scroll_to_y(mouth_area, LV_COORD_MAX, LV_ANIM_OFF);
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
    (void)url;
    const char *ssid = "MiaomiaoClaw";
    char hint_copy[192] = {0};
    if (hint && hint[0]) {
        snprintf(hint_copy, sizeof(hint_copy), "%s", hint);
        const char *prefix = "热点: ";
        char *ssid_start = strstr(hint_copy, prefix);
        if (ssid_start) {
            ssid_start += strlen(prefix);
            char *ssid_end = strchr(ssid_start, '\n');
            if (ssid_end) *ssid_end = '\0';
            if (ssid_start[0]) ssid = ssid_start;
        }
    }

    if (!lvgl_port_lock(500)) return;

    if (qr_overlay) {
        lv_obj_delete(qr_overlay);
        qr_overlay = NULL;
    }

    qr_overlay = lv_obj_create(screen);
    lv_obj_set_size(qr_overlay, LCD_H_RES, LCD_V_RES);
    lv_obj_align(qr_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(qr_overlay, lv_color_hex(0x000010), 0);
    lv_obj_set_style_bg_opa(qr_overlay, LV_OPA_90, 0);
    lv_obj_set_style_border_width(qr_overlay, 0, 0);
    lv_obj_set_style_pad_all(qr_overlay, 0, 0);
    lv_obj_clear_flag(qr_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(qr_overlay);
    lv_obj_set_style_text_font(title, font_cn, 0);
    lv_obj_set_style_text_color(title, C_CYAN, 0);
    lv_label_set_text(title, "进入配网模式");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);

    lv_obj_t *steps = lv_label_create(qr_overlay);
    lv_obj_set_width(steps, LCD_H_RES - 36);
    lv_obj_set_style_text_font(steps, font_cn, 0);
    lv_obj_set_style_text_color(steps, lv_color_hex(0xE8F0FF), 0);
    lv_label_set_long_mode(steps, LV_LABEL_LONG_WRAP);
    lv_label_set_text_fmt(steps,
        "1. 连接设备热点 %s\n"
        "2. 打开手机浏览器\n"
        "3. 访问 http://192.168.4.1\n"
        "   进入web配置面板",
        ssid);
    lv_obj_align(steps, LV_ALIGN_CENTER, 0, -8);

    lv_obj_t *hint_lbl = lv_label_create(qr_overlay);
    lv_obj_set_width(hint_lbl, LCD_H_RES - 36);
    lv_obj_set_style_text_font(hint_lbl, font_cn, 0);
    lv_obj_set_style_text_color(hint_lbl, lv_color_hex(0xA0C0FF), 0);
    lv_label_set_long_mode(hint_lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(hint_lbl, hint ? hint : "热点: MiaomiaoClaw\n访问 http://192.168.4.1 进入web配置面板");
    lv_obj_align(hint_lbl, LV_ALIGN_BOTTOM_MID, 0, -16);

    lvgl_port_unlock();
}

void lcd_hide_qr_overlay(void)
{
    if (!lvgl_port_lock(500)) return;
    if (qr_overlay) {
        lv_obj_delete(qr_overlay);
        qr_overlay = NULL;
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

    // 构建 Otto 机器人 UI
    build_robot_ui();

    // ── 对话区 ──────────────────────────────────────────────
    mouth_area = lv_obj_create(screen);
    lv_obj_set_size(mouth_area, LCD_H_RES - 8, LCD_V_RES - 132 - 22);
    lv_obj_align(mouth_area, LV_ALIGN_TOP_MID, 0, 134);
    lv_obj_set_style_bg_color(mouth_area, C_MOUTH_BG, 0);
    lv_obj_set_style_bg_opa(mouth_area, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mouth_area, 8, 0);
    lv_obj_set_style_border_width(mouth_area, 1, 0);
    lv_obj_set_style_border_color(mouth_area, C_MOUTH_BORDER, 0);
    lv_obj_set_style_border_opa(mouth_area, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(mouth_area, 5, 0);
    lv_obj_set_scrollbar_mode(mouth_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(mouth_area, LV_DIR_VER);
    lv_obj_add_flag(mouth_area, LV_OBJ_FLAG_SCROLLABLE);
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

    bl_ledc_init();
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
        current_state = LCD_STATE_SLEEPING;
        base_mood = LCD_STATE_SLEEPING;
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
    if (!lvgl_port_lock(500)) return;
    apply_state_locked(state);
    lvgl_port_unlock();
}

void lcd_set_base_mood(lcd_state_t state)
{
    if (!is_valid_base_mood(state)) return;
    if (!lvgl_port_lock(500)) return;
    base_mood = state;
    if (!is_transient_state(current_state)) {
        apply_state_locked(base_mood);
    }
    lvgl_port_unlock();
}

lcd_state_t lcd_get_base_mood(void)
{
    return base_mood;
}

void lcd_restore_base_mood(void)
{
    if (!lvgl_port_lock(500)) return;
    apply_state_locked(base_mood);
    lvgl_port_unlock();
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

