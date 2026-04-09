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

// ── 硬件引脚 ──────────────────────────────────────────────
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

// ── 颜色 ─────────────────────────────────────────────────
#define COLOR_BG            lv_color_hex(0x080818)
#define COLOR_FACE_BG       lv_color_hex(0x0C0C24)
#define COLOR_MOUTH_BG      lv_color_hex(0x0E0E28)
#define COLOR_MOUTH_BORDER  lv_color_hex(0x1A1A4A)
#define COLOR_TEXT          lv_color_hex(0xD0D0F0)
#define COLOR_TEXT_USER     lv_color_hex(0x80FFB0)
#define COLOR_TEXT_ASSIST   lv_color_hex(0xB0B0FF)
#define COLOR_TEXT_DIM      lv_color_hex(0x606080)
#define COLOR_WIFI          lv_color_hex(0x4080A0)
#define COLOR_ERROR         lv_color_hex(0xFF4040)

// 眼睛颜色
#define EYE_COLOR_CYAN      lv_color_hex(0x00E5FF)
#define EYE_COLOR_PURPLE    lv_color_hex(0x7B2FF7)
#define EYE_COLOR_GREEN     lv_color_hex(0x00FF88)
#define EYE_COLOR_ORANGE    lv_color_hex(0xFFAA00)
#define EYE_COLOR_RED       lv_color_hex(0xFF4040)
#define EYE_COLOR_PINK      lv_color_hex(0xFF69B4)
#define EYE_COLOR_YELLOW    lv_color_hex(0xFFFF00)
#define EYE_COLOR_WHITE     lv_color_hex(0xFFFFFF)
#define EYE_COLOR_DIM       lv_color_hex(0x1A2A3A)
#define EYE_COLOR_GOLD      lv_color_hex(0xFFD700)
#define EYE_COLOR_LIME      lv_color_hex(0x39FF14)

// ── 眼睛尺寸 ─────────────────────────────────────────────
#define EYE_R       36   // 眼球半径（直径72）
#define PUPIL_R     18   // 瞳孔半径（直径36）
#define EYE_Y_OFF   0    // 眼睛垂直偏移

// ── LVGL 对象 ────────────────────────────────────────────
static esp_lcd_panel_io_handle_t lcd_io    = NULL;
static esp_lcd_panel_handle_t    lcd_panel = NULL;
static lv_display_t             *lvgl_disp = NULL;

static lv_obj_t *screen       = NULL;
static lv_obj_t *face_area    = NULL;

// 左眼
static lv_obj_t *l_eye        = NULL;   // 眼白（圆）
static lv_obj_t *l_pupil      = NULL;   // 瞳孔
static lv_obj_t *l_shine      = NULL;   // 高光点
static lv_obj_t *l_lid_top    = NULL;   // 上眼皮（遮罩）
static lv_obj_t *l_lid_bot    = NULL;   // 下眼皮（遮罩）
static lv_obj_t *l_sweat      = NULL;   // 汗珠（害羞/健身）
static lv_obj_t *l_star       = NULL;   // 星星（恋爱）
static lv_obj_t *l_spiral     = NULL;   // 螺旋（发晕）

// 右眼
static lv_obj_t *r_eye        = NULL;
static lv_obj_t *r_pupil      = NULL;
static lv_obj_t *r_shine      = NULL;
static lv_obj_t *r_lid_top    = NULL;
static lv_obj_t *r_lid_bot    = NULL;
static lv_obj_t *r_sweat      = NULL;
static lv_obj_t *r_star       = NULL;
static lv_obj_t *r_spiral     = NULL;

// 嘴巴 / 状态区
static lv_obj_t *mouth_area   = NULL;
static lv_obj_t *mouth_label  = NULL;
static lv_obj_t *wifi_icon    = NULL;
static lv_obj_t *status_label = NULL;   // WiFi左边的状态文字

static lcd_state_t current_state = LCD_STATE_SLEEPING;
static const lv_font_t *font_text  = NULL;
static const lv_font_t *font_small = NULL;

// 随机情绪触发
static uint32_t mood_timer_ticks = 0;

// ── 状态文字映射 ─────────────────────────────────────────
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
};

// ═══════════════════════════════════════════════════════════
//  工具函数
// ═══════════════════════════════════════════════════════════

void lcd_backlight_set(bool on)
{
    gpio_set_level(PIN_BL, on ? 1 : 0);
}

// 停止所有动画
static void stop_all_anims(void)
{
    lv_obj_t *objs[] = {
        l_eye, l_pupil, l_shine, l_lid_top, l_lid_bot, l_sweat, l_star, l_spiral,
        r_eye, r_pupil, r_shine, r_lid_top, r_lid_bot, r_sweat, r_star, r_spiral,
        mouth_area, face_area
    };
    for (int i = 0; i < (int)(sizeof(objs)/sizeof(objs[0])); i++) {
        if (objs[i]) lv_anim_delete(objs[i], NULL);
    }
}

// 隐藏所有装饰物（星星/汗珠/螺旋）
static void hide_decorations(void)
{
    lv_obj_t *deco[] = { l_sweat, r_sweat, l_star, r_star, l_spiral, r_spiral };
    for (int i = 0; i < 6; i++) {
        if (deco[i]) lv_obj_add_flag(deco[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// 重置眼皮（完全打开）
static void reset_lids(void)
{
    if (l_lid_top) { lv_obj_set_height(l_lid_top, 0); lv_obj_align(l_lid_top, LV_ALIGN_TOP_MID, 0, 0); }
    if (r_lid_top) { lv_obj_set_height(r_lid_top, 0); lv_obj_align(r_lid_top, LV_ALIGN_TOP_MID, 0, 0); }
    if (l_lid_bot) { lv_obj_set_height(l_lid_bot, 0); lv_obj_align(l_lid_bot, LV_ALIGN_BOTTOM_MID, 0, 0); }
    if (r_lid_bot) { lv_obj_set_height(r_lid_bot, 0); lv_obj_align(r_lid_bot, LV_ALIGN_BOTTOM_MID, 0, 0); }
}

// 重置瞳孔大小和位置
static void reset_pupils(void)
{
    if (l_pupil) { lv_obj_set_size(l_pupil, PUPIL_R*2, PUPIL_R*2); lv_obj_set_style_radius(l_pupil, PUPIL_R, 0); lv_obj_align(l_pupil, LV_ALIGN_CENTER, 0, 0); }
    if (r_pupil) { lv_obj_set_size(r_pupil, PUPIL_R*2, PUPIL_R*2); lv_obj_set_style_radius(r_pupil, PUPIL_R, 0); lv_obj_align(r_pupil, LV_ALIGN_CENTER, 0, 0); }
    if (l_shine) { lv_obj_set_size(l_shine, 8, 8); lv_obj_align(l_shine, LV_ALIGN_TOP_RIGHT, -4, 4); }
    if (r_shine) { lv_obj_set_size(r_shine, 8, 8); lv_obj_align(r_shine, LV_ALIGN_TOP_RIGHT, -4, 4); }
}

// 设置眼睛颜色
static void set_eye_color(lv_color_t eye_col, lv_color_t pupil_col)
{
    lv_obj_t *eyes[]   = { l_eye,   r_eye   };
    lv_obj_t *pupils[] = { l_pupil, r_pupil };
    for (int i = 0; i < 2; i++) {
        if (eyes[i])   lv_obj_set_style_bg_color(eyes[i],   eye_col,   0);
        if (pupils[i]) lv_obj_set_style_bg_color(pupils[i], pupil_col, 0);
    }
}

// 动画回调
static void anim_opa_cb(void *var, int32_t v)   { lv_obj_set_style_bg_opa((lv_obj_t*)var, (lv_opa_t)v, 0); }
static void anim_w_cb(void *var, int32_t v)     { lv_obj_set_width((lv_obj_t*)var, v); }
static void anim_h_cb(void *var, int32_t v)     { lv_obj_set_height((lv_obj_t*)var, v); }
static void anim_x_cb(void *var, int32_t v)     { lv_obj_set_x((lv_obj_t*)var, v); }
static void anim_y_cb(void *var, int32_t v)     { lv_obj_set_y((lv_obj_t*)var, v); }
// 快速创建无限循环动画
static void make_anim_inf(lv_obj_t *obj, lv_anim_exec_xcb_t cb,
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

// 单次动画（带 playback）
static void make_anim_once(lv_obj_t *obj, lv_anim_exec_xcb_t cb,
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

// ═══════════════════════════════════════════════════════════
//  眼睛创建
// ═══════════════════════════════════════════════════════════

// 创建单个眼睛（圆形眼白 + 瞳孔 + 高光 + 上下眼皮 + 装饰）
// cx: 在 face_area 中的 x 中心偏移（相对 face_area 中心）
static void create_eye(bool is_left, int32_t cx)
{
    int32_t d = EYE_R * 2;  // 72

    // 眼白
    lv_obj_t *eye = lv_obj_create(face_area);
    lv_obj_set_size(eye, d, d);
    lv_obj_set_style_radius(eye, EYE_R, 0);
    lv_obj_set_style_bg_color(eye, EYE_COLOR_CYAN, 0);
    lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(eye, 0, 0);
    lv_obj_set_style_pad_all(eye, 0, 0);
    lv_obj_set_scrollbar_mode(eye, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(eye, LV_ALIGN_CENTER, cx, EYE_Y_OFF);

    // 瞳孔
    lv_obj_t *pupil = lv_obj_create(eye);
    lv_obj_set_size(pupil, PUPIL_R*2, PUPIL_R*2);
    lv_obj_set_style_radius(pupil, PUPIL_R, 0);
    lv_obj_set_style_bg_color(pupil, lv_color_hex(0x001830), 0);
    lv_obj_set_style_bg_opa(pupil, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(pupil, 0, 0);
    lv_obj_set_style_pad_all(pupil, 0, 0);
    lv_obj_set_scrollbar_mode(pupil, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(pupil, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(pupil, LV_ALIGN_CENTER, 0, 0);

    // 高光
    lv_obj_t *shine = lv_obj_create(eye);
    lv_obj_set_size(shine, 8, 8);
    lv_obj_set_style_radius(shine, 4, 0);
    lv_obj_set_style_bg_color(shine, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(shine, LV_OPA_80, 0);
    lv_obj_set_style_border_width(shine, 0, 0);
    lv_obj_set_style_pad_all(shine, 0, 0);
    lv_obj_set_scrollbar_mode(shine, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(shine, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(shine, LV_ALIGN_TOP_RIGHT, -4, 4);

    // 上眼皮（从顶部向下覆盖，高度0=全开）
    lv_obj_t *lid_top = lv_obj_create(eye);
    lv_obj_set_size(lid_top, d, 0);
    lv_obj_set_style_radius(lid_top, 0, 0);
    lv_obj_set_style_bg_color(lid_top, COLOR_FACE_BG, 0);
    lv_obj_set_style_bg_opa(lid_top, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lid_top, 0, 0);
    lv_obj_set_style_pad_all(lid_top, 0, 0);
    lv_obj_set_scrollbar_mode(lid_top, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(lid_top, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(lid_top, LV_ALIGN_TOP_MID, 0, 0);

    // 下眼皮（从底部向上覆盖）
    lv_obj_t *lid_bot = lv_obj_create(eye);
    lv_obj_set_size(lid_bot, d, 0);
    lv_obj_set_style_radius(lid_bot, 0, 0);
    lv_obj_set_style_bg_color(lid_bot, COLOR_FACE_BG, 0);
    lv_obj_set_style_bg_opa(lid_bot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lid_bot, 0, 0);
    lv_obj_set_style_pad_all(lid_bot, 0, 0);
    lv_obj_set_scrollbar_mode(lid_bot, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(lid_bot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(lid_bot, LV_ALIGN_BOTTOM_MID, 0, 0);

    // 汗珠（右上角小圆，默认隐藏）
    lv_obj_t *sweat = lv_obj_create(face_area);
    lv_obj_set_size(sweat, 8, 12);
    lv_obj_set_style_radius(sweat, 4, 0);
    lv_obj_set_style_bg_color(sweat, lv_color_hex(0x00BFFF), 0);
    lv_obj_set_style_bg_opa(sweat, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sweat, 0, 0);
    lv_obj_set_style_pad_all(sweat, 0, 0);
    lv_obj_set_scrollbar_mode(sweat, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(sweat, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(sweat, LV_ALIGN_CENTER, cx + (is_left ? 22 : 22), EYE_Y_OFF - 28);
    lv_obj_add_flag(sweat, LV_OBJ_FLAG_HIDDEN);

    // 星星（恋爱，用小方块模拟）
    lv_obj_t *star = lv_obj_create(face_area);
    lv_obj_set_size(star, 14, 14);
    lv_obj_set_style_radius(star, 2, 0);
    lv_obj_set_style_bg_color(star, EYE_COLOR_PINK, 0);
    lv_obj_set_style_bg_opa(star, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(star, 0, 0);
    lv_obj_set_style_pad_all(star, 0, 0);
    lv_obj_set_scrollbar_mode(star, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(star, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(star, LV_ALIGN_CENTER, cx + (is_left ? -28 : 28), EYE_Y_OFF - 32);
    lv_obj_add_flag(star, LV_OBJ_FLAG_HIDDEN);

    // 螺旋（发晕，用小圆模拟）
    lv_obj_t *spiral = lv_obj_create(face_area);
    lv_obj_set_size(spiral, 10, 10);
    lv_obj_set_style_radius(spiral, 5, 0);
    lv_obj_set_style_bg_color(spiral, EYE_COLOR_YELLOW, 0);
    lv_obj_set_style_bg_opa(spiral, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(spiral, 2, 0);
    lv_obj_set_style_border_color(spiral, lv_color_hex(0xFF8800), 0);
    lv_obj_set_style_pad_all(spiral, 0, 0);
    lv_obj_set_scrollbar_mode(spiral, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(spiral, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(spiral, LV_ALIGN_CENTER, cx, EYE_Y_OFF);
    lv_obj_add_flag(spiral, LV_OBJ_FLAG_HIDDEN);

    if (is_left) {
        l_eye = eye; l_pupil = pupil; l_shine = shine;
        l_lid_top = lid_top; l_lid_bot = lid_bot;
        l_sweat = sweat; l_star = star; l_spiral = spiral;
    } else {
        r_eye = eye; r_pupil = pupil; r_shine = shine;
        r_lid_top = lid_top; r_lid_bot = lid_bot;
        r_sweat = sweat; r_star = star; r_spiral = spiral;
    }
}

// ═══════════════════════════════════════════════════════════
//  各状态表情实现
// ═══════════════════════════════════════════════════════════

// 眨眼（通用，上眼皮快速关闭再打开）
static void do_blink(void)
{
    make_anim_once(l_lid_top, anim_h_cb, 0, EYE_R*2, 100, 100);
    make_anim_once(r_lid_top, anim_h_cb, 0, EYE_R*2, 100, 100);
}

// 定时眨眼回调
static void blink_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (current_state == LCD_STATE_SLEEPING) return;
    if (lvgl_port_lock(0)) {
        do_blink();
        lvgl_port_unlock();
    }
}

// ── 睡觉：上眼皮半闭，慢速呼吸光 ──────────────────────────
static void expr_sleeping(void)
{
    set_eye_color(EYE_COLOR_DIM, lv_color_hex(0x0A1520));
    // 上眼皮盖住 2/3
    lv_obj_set_height(l_lid_top, EYE_R + 10);
    lv_obj_set_height(r_lid_top, EYE_R + 10);
    lv_obj_align(l_lid_top, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_align(r_lid_top, LV_ALIGN_TOP_MID, 0, 0);
    // 下眼皮微微上来
    lv_obj_set_height(l_lid_bot, 8);
    lv_obj_set_height(r_lid_bot, 8);
    // 慢速呼吸：眼睛透明度脉动
    make_anim_inf(l_eye, anim_opa_cb, LV_OPA_20, LV_OPA_50, 2000, 2000);
    make_anim_inf(r_eye, anim_opa_cb, LV_OPA_20, LV_OPA_50, 2000, 2000);
    // 高光隐藏
    lv_obj_set_style_bg_opa(l_shine, LV_OPA_0, 0);
    lv_obj_set_style_bg_opa(r_shine, LV_OPA_0, 0);
}

// ── 联网中：橙色眼睛，瞳孔左右扫描 ───────────────────────
static void expr_connecting(void)
{
    set_eye_color(EYE_COLOR_ORANGE, lv_color_hex(0x3A1800));
    make_anim_inf(l_pupil, anim_x_cb, -12, 12, 500, 500);
    make_anim_inf(r_pupil, anim_x_cb, -12, 12, 500, 500);
    make_anim_inf(l_eye, anim_opa_cb, LV_OPA_60, LV_OPA_100, 400, 400);
    make_anim_inf(r_eye, anim_opa_cb, LV_OPA_60, LV_OPA_100, 400, 400);
}

// ── 已连接：青色，瞳孔弹跳一下 ────────────────────────────
static void expr_connected(void)
{
    set_eye_color(EYE_COLOR_CYAN, lv_color_hex(0x001830));
    make_anim_once(l_pupil, anim_h_cb, PUPIL_R*2, PUPIL_R*2+10, 150, 150);
    make_anim_once(r_pupil, anim_h_cb, PUPIL_R*2, PUPIL_R*2+10, 150, 150);
}

// ── 网络挂了：红色，眼睛抖动 ──────────────────────────────
static void expr_error(void)
{
    set_eye_color(EYE_COLOR_RED, lv_color_hex(0x3A0000));
    // 上眼皮皱眉（内侧压低）
    lv_obj_set_height(l_lid_top, 14);
    lv_obj_set_height(r_lid_top, 14);
    lv_obj_align(l_lid_top, LV_ALIGN_TOP_MID, 6, 0);   // 左眼皮偏右（皱眉）
    lv_obj_align(r_lid_top, LV_ALIGN_TOP_MID, -6, 0);  // 右眼皮偏左
    // 抖动
    make_anim_inf(l_eye, anim_x_cb,
        lv_obj_get_x(l_eye)-3, lv_obj_get_x(l_eye)+3, 60, 60);
    make_anim_inf(r_eye, anim_x_cb,
        lv_obj_get_x(r_eye)-3, lv_obj_get_x(r_eye)+3, 60, 60);
}

// ── 思考中：紫色，瞳孔缓慢转圈（用Y轴模拟） ──────────────
static void expr_thinking(void)
{
    set_eye_color(EYE_COLOR_PURPLE, lv_color_hex(0x1A0040));
    // 瞳孔上下漂移（思考感）
    make_anim_inf(l_pupil, anim_y_cb, -8, 8, 800, 800);
    make_anim_inf(r_pupil, anim_y_cb, -8, 8, 1000, 1000);
    // 眼睛微微脉动
    make_anim_inf(l_eye, anim_opa_cb, LV_OPA_70, LV_OPA_100, 600, 600);
    make_anim_inf(r_eye, anim_opa_cb, LV_OPA_70, LV_OPA_100, 600, 600);
    // 上眼皮微微压低（专注感）
    lv_obj_set_height(l_lid_top, 8);
    lv_obj_set_height(r_lid_top, 8);
}

// ── 在码字：青色，瞳孔快速左右扫（打字感） ───────────────
static void expr_speaking(void)
{
    set_eye_color(EYE_COLOR_CYAN, lv_color_hex(0x001830));
    make_anim_inf(l_pupil, anim_x_cb, -10, 10, 200, 50);
    make_anim_inf(r_pupil, anim_x_cb, -10, 10, 200, 50);
    // 眼睛轻微缩放（打字节奏）
    make_anim_inf(l_eye, anim_w_cb, EYE_R*2-4, EYE_R*2+4, 150, 150);
    make_anim_inf(r_eye, anim_w_cb, EYE_R*2-4, EYE_R*2+4, 150, 150);
}

// ── 在听：绿色，瞳孔放大脉动 ──────────────────────────────
static void expr_listening(void)
{
    set_eye_color(EYE_COLOR_GREEN, lv_color_hex(0x001A0A));
    make_anim_inf(l_pupil, anim_h_cb, PUPIL_R*2-6, PUPIL_R*2+8, 400, 400);
    make_anim_inf(r_pupil, anim_h_cb, PUPIL_R*2-6, PUPIL_R*2+8, 400, 400);
    make_anim_inf(l_pupil, anim_w_cb, PUPIL_R*2-6, PUPIL_R*2+8, 400, 400);
    make_anim_inf(r_pupil, anim_w_cb, PUPIL_R*2-6, PUPIL_R*2+8, 400, 400);
}

// ── 开心：黄色，眼睛变弯（下眼皮上来，上眼皮压低） ────────
static void expr_happy(void)
{
    set_eye_color(EYE_COLOR_YELLOW, lv_color_hex(0x1A1400));
    lv_obj_set_height(l_lid_bot, 20);
    lv_obj_set_height(r_lid_bot, 20);
    lv_obj_set_height(l_lid_top, 6);
    lv_obj_set_height(r_lid_top, 6);
    // 眼睛跳动
    make_anim_inf(l_eye, anim_y_cb,
        lv_obj_get_y(l_eye)-4, lv_obj_get_y(l_eye)+4, 300, 300);
    make_anim_inf(r_eye, anim_y_cb,
        lv_obj_get_y(r_eye)-4, lv_obj_get_y(r_eye)+4, 300, 300);
}

// ── 神游：青色半透明，瞳孔漂移到角落 ─────────────────────
static void expr_daydream(void)
{
    set_eye_color(EYE_COLOR_CYAN, lv_color_hex(0x001830));
    lv_obj_set_style_bg_opa(l_eye, LV_OPA_50, 0);
    lv_obj_set_style_bg_opa(r_eye, LV_OPA_50, 0);
    // 瞳孔漂到右上角
    lv_obj_align(l_pupil, LV_ALIGN_TOP_RIGHT, -4, 4);
    lv_obj_align(r_pupil, LV_ALIGN_TOP_RIGHT, -4, 4);
    // 缓慢漂移
    make_anim_inf(l_pupil, anim_x_cb, 4, 12, 3000, 3000);
    make_anim_inf(r_pupil, anim_x_cb, 4, 12, 3000, 3000);
    // 眼睛缓慢呼吸
    make_anim_inf(l_eye, anim_opa_cb, LV_OPA_40, LV_OPA_70, 2500, 2500);
    make_anim_inf(r_eye, anim_opa_cb, LV_OPA_40, LV_OPA_70, 2500, 2500);
}

// ── 贪恋爱：粉色，星星装饰，瞳孔变心形（用圆模拟） ────────
static void expr_in_love(void)
{
    set_eye_color(EYE_COLOR_PINK, lv_color_hex(0x3A0020));
    // 显示星星
    lv_obj_clear_flag(l_star, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(r_star, LV_OBJ_FLAG_HIDDEN);
    // 星星闪烁
    make_anim_inf(l_star, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 500, 500);
    make_anim_inf(r_star, anim_opa_cb, LV_OPA_0, LV_OPA_COVER, 700, 700);
    // 眼睛脉动
    make_anim_inf(l_eye, anim_w_cb, EYE_R*2-6, EYE_R*2+6, 600, 600);
    make_anim_inf(r_eye, anim_w_cb, EYE_R*2-6, EYE_R*2+6, 600, 600);
    make_anim_inf(l_eye, anim_h_cb, EYE_R*2-6, EYE_R*2+6, 600, 600);
    make_anim_inf(r_eye, anim_h_cb, EYE_R*2-6, EYE_R*2+6, 600, 600);
    lv_obj_set_height(l_lid_top, 4);
    lv_obj_set_height(r_lid_top, 4);
}

// ── 吃饭：橙色，瞳孔上下咀嚼 ─────────────────────────────
static void expr_eating(void)
{
    set_eye_color(EYE_COLOR_ORANGE, lv_color_hex(0x1A0800));
    // 下眼皮上下动（咀嚼）
    make_anim_inf(l_lid_bot, anim_h_cb, 0, 18, 300, 300);
    make_anim_inf(r_lid_bot, anim_h_cb, 0, 18, 300, 300);
    // 瞳孔微微下移（看食物）
    lv_obj_align(l_pupil, LV_ALIGN_CENTER, 0, 6);
    lv_obj_align(r_pupil, LV_ALIGN_CENTER, 0, 6);
}

// ── 健身：绿色，眼睛用力眯起，汗珠 ───────────────────────
static void expr_workout(void)
{
    set_eye_color(EYE_COLOR_LIME, lv_color_hex(0x001A00));
    lv_obj_set_height(l_lid_top, 20);
    lv_obj_set_height(r_lid_top, 20);
    lv_obj_set_height(l_lid_bot, 10);
    lv_obj_set_height(r_lid_bot, 10);
    // 显示汗珠并下落
    lv_obj_clear_flag(l_sweat, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(r_sweat, LV_OBJ_FLAG_HIDDEN);
    make_anim_inf(l_sweat, anim_y_cb,
        lv_obj_get_y(l_sweat), lv_obj_get_y(l_sweat)+20, 800, 200);
    make_anim_inf(r_sweat, anim_y_cb,
        lv_obj_get_y(r_sweat), lv_obj_get_y(r_sweat)+20, 800, 200);
    // 眼睛抖动（用力感）
    make_anim_inf(l_eye, anim_y_cb,
        lv_obj_get_y(l_eye)-2, lv_obj_get_y(l_eye)+2, 100, 100);
    make_anim_inf(r_eye, anim_y_cb,
        lv_obj_get_y(r_eye)-2, lv_obj_get_y(r_eye)+2, 100, 100);
}

// ── 学习：蓝色，瞳孔上移（看书感），上眼皮微压 ────────────
static void expr_studying(void)
{
    set_eye_color(lv_color_hex(0x4488FF), lv_color_hex(0x001040));
    lv_obj_align(l_pupil, LV_ALIGN_CENTER, 0, -6);
    lv_obj_align(r_pupil, LV_ALIGN_CENTER, 0, -6);
    lv_obj_set_height(l_lid_top, 10);
    lv_obj_set_height(r_lid_top, 10);
    // 缓慢左右扫（读书）
    make_anim_inf(l_pupil, anim_x_cb, -8, 8, 1500, 1500);
    make_anim_inf(r_pupil, anim_x_cb, -8, 8, 1500, 1500);
}

// ── 看电视：白色大眼，瞳孔随机跳动 ───────────────────────
static void expr_watching_tv(void)
{
    set_eye_color(lv_color_hex(0xE0E8FF), lv_color_hex(0x000820));
    // 大瞳孔（入迷）
    lv_obj_set_size(l_pupil, PUPIL_R*2+10, PUPIL_R*2+10);
    lv_obj_set_size(r_pupil, PUPIL_R*2+10, PUPIL_R*2+10);
    // 瞳孔左右跳（跟着画面）
    make_anim_inf(l_pupil, anim_x_cb, -10, 10, 1200, 100);
    make_anim_inf(r_pupil, anim_x_cb, -10, 10, 1200, 100);
    // 眼睛微微放大
    make_anim_inf(l_eye, anim_w_cb, EYE_R*2, EYE_R*2+8, 2000, 2000);
    make_anim_inf(r_eye, anim_w_cb, EYE_R*2, EYE_R*2+8, 2000, 2000);
}

// ── 不想理你：半闭眼，瞳孔看向一侧 ───────────────────────
static void expr_ignoring(void)
{
    set_eye_color(lv_color_hex(0x607080), lv_color_hex(0x101820));
    lv_obj_set_height(l_lid_top, EYE_R + 4);
    lv_obj_set_height(r_lid_top, EYE_R + 4);
    // 瞳孔看向右边（无视你）
    lv_obj_align(l_pupil, LV_ALIGN_CENTER, 10, 0);
    lv_obj_align(r_pupil, LV_ALIGN_CENTER, 10, 0);
    lv_obj_set_style_bg_opa(l_shine, LV_OPA_0, 0);
    lv_obj_set_style_bg_opa(r_shine, LV_OPA_0, 0);
}

// ── 生气：红色，眉毛压低，眼睛抖动 ───────────────────────
static void expr_angry(void)
{
    set_eye_color(EYE_COLOR_RED, lv_color_hex(0x2A0000));
    lv_obj_set_height(l_lid_top, 18);
    lv_obj_set_height(r_lid_top, 18);
    lv_obj_align(l_lid_top, LV_ALIGN_TOP_MID, 8, 0);
    lv_obj_align(r_lid_top, LV_ALIGN_TOP_MID, -8, 0);
    make_anim_inf(l_eye, anim_x_cb,
        lv_obj_get_x(l_eye)-4, lv_obj_get_x(l_eye)+4, 80, 80);
    make_anim_inf(r_eye, anim_x_cb,
        lv_obj_get_x(r_eye)-4, lv_obj_get_x(r_eye)+4, 80, 80);
    make_anim_inf(l_eye, anim_opa_cb, LV_OPA_80, LV_OPA_100, 200, 200);
    make_anim_inf(r_eye, anim_opa_cb, LV_OPA_80, LV_OPA_100, 200, 200);
}

// ── 惊讶：白色超大眼，瞳孔缩小 ────────────────────────────
static void expr_surprised(void)
{
    set_eye_color(lv_color_hex(0xFFFFFF), lv_color_hex(0x000010));
    // 眼睛放大
    lv_obj_set_size(l_eye, EYE_R*2+16, EYE_R*2+16);
    lv_obj_set_size(r_eye, EYE_R*2+16, EYE_R*2+16);
    lv_obj_set_style_radius(l_eye, EYE_R+8, 0);
    lv_obj_set_style_radius(r_eye, EYE_R+8, 0);
    // 瞳孔缩小
    lv_obj_set_size(l_pupil, 12, 12);
    lv_obj_set_size(r_pupil, 12, 12);
    lv_obj_set_style_radius(l_pupil, 6, 0);
    lv_obj_set_style_radius(r_pupil, 6, 0);
    // 快速脉动
    make_anim_inf(l_eye, anim_opa_cb, LV_OPA_80, LV_OPA_100, 300, 300);
    make_anim_inf(r_eye, anim_opa_cb, LV_OPA_80, LV_OPA_100, 300, 300);
}

// ── 无聊：灰色，眼皮半闭，瞳孔缓慢下移 ───────────────────
static void expr_bored(void)
{
    set_eye_color(lv_color_hex(0x505060), lv_color_hex(0x101015));
    lv_obj_set_height(l_lid_top, EYE_R - 4);
    lv_obj_set_height(r_lid_top, EYE_R - 4);
    lv_obj_align(l_pupil, LV_ALIGN_CENTER, 0, 8);
    lv_obj_align(r_pupil, LV_ALIGN_CENTER, 0, 8);
    make_anim_inf(l_pupil, anim_y_cb, 6, 12, 2000, 2000);
    make_anim_inf(r_pupil, anim_y_cb, 6, 12, 2000, 2000);
    lv_obj_set_style_bg_opa(l_shine, LV_OPA_0, 0);
    lv_obj_set_style_bg_opa(r_shine, LV_OPA_0, 0);
}

// ── 赛博模式：绿色矩阵风，瞳孔变方形，快速扫描 ────────────
static void expr_cyber(void)
{
    set_eye_color(EYE_COLOR_LIME, lv_color_hex(0x001400));
    // 瞳孔变方形
    lv_obj_set_style_radius(l_pupil, 2, 0);
    lv_obj_set_style_radius(r_pupil, 2, 0);
    lv_obj_set_size(l_pupil, PUPIL_R*2+4, PUPIL_R*2-8);
    lv_obj_set_size(r_pupil, PUPIL_R*2+4, PUPIL_R*2-8);
    // 快速扫描
    make_anim_inf(l_pupil, anim_x_cb, -12, 12, 150, 50);
    make_anim_inf(r_pupil, anim_x_cb, -12, 12, 150, 50);
    // 眼睛闪烁
    make_anim_inf(l_eye, anim_opa_cb, LV_OPA_60, LV_OPA_100, 100, 100);
    make_anim_inf(r_eye, anim_opa_cb, LV_OPA_60, LV_OPA_100, 100, 100);
    // 眼睛变方形
    lv_obj_set_style_radius(l_eye, 8, 0);
    lv_obj_set_style_radius(r_eye, 8, 0);
}

// ── 发晕：黄色，眼睛旋转（用X轴模拟），螺旋装饰 ──────────
static void expr_dizzy(void)
{
    set_eye_color(EYE_COLOR_YELLOW, lv_color_hex(0x1A1400));
    lv_obj_clear_flag(l_spiral, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(r_spiral, LV_OBJ_FLAG_HIDDEN);
    // 螺旋旋转（用大小脉动模拟）
    make_anim_inf(l_spiral, anim_w_cb, 6, 18, 400, 400);
    make_anim_inf(r_spiral, anim_w_cb, 6, 18, 400, 400);
    make_anim_inf(l_spiral, anim_h_cb, 6, 18, 400, 400);
    make_anim_inf(r_spiral, anim_h_cb, 6, 18, 400, 400);
    // 眼睛摇晃
    make_anim_inf(l_eye, anim_x_cb,
        lv_obj_get_x(l_eye)-6, lv_obj_get_x(l_eye)+6, 200, 200);
    make_anim_inf(r_eye, anim_x_cb,
        lv_obj_get_x(r_eye)-6, lv_obj_get_x(r_eye)+6, 200, 200);
    lv_obj_set_style_bg_opa(l_pupil, LV_OPA_0, 0);
    lv_obj_set_style_bg_opa(r_pupil, LV_OPA_0, 0);
}

// ── 害羞：粉色，眼睛向下看，汗珠 ─────────────────────────
static void expr_shy(void)
{
    set_eye_color(EYE_COLOR_PINK, lv_color_hex(0x200010));
    lv_obj_set_height(l_lid_top, 12);
    lv_obj_set_height(r_lid_top, 12);
    lv_obj_align(l_pupil, LV_ALIGN_CENTER, -4, 8);
    lv_obj_align(r_pupil, LV_ALIGN_CENTER, -4, 8);
    lv_obj_clear_flag(l_sweat, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(r_sweat, LV_OBJ_FLAG_HIDDEN);
    make_anim_inf(l_sweat, anim_opa_cb, LV_OPA_40, LV_OPA_COVER, 800, 800);
    make_anim_inf(r_sweat, anim_opa_cb, LV_OPA_40, LV_OPA_COVER, 800, 800);
    // 眼睛微微脉动（心跳）
    make_anim_inf(l_eye, anim_opa_cb, LV_OPA_70, LV_OPA_100, 500, 500);
    make_anim_inf(r_eye, anim_opa_cb, LV_OPA_70, LV_OPA_100, 500, 500);
}

// ── 亢奋：金色，眼睛快速放大缩小 ─────────────────────────
static void expr_excited(void)
{
    set_eye_color(EYE_COLOR_GOLD, lv_color_hex(0x1A1000));
    make_anim_inf(l_eye, anim_w_cb, EYE_R*2-10, EYE_R*2+10, 200, 200);
    make_anim_inf(r_eye, anim_w_cb, EYE_R*2-10, EYE_R*2+10, 200, 200);
    make_anim_inf(l_eye, anim_h_cb, EYE_R*2-10, EYE_R*2+10, 200, 200);
    make_anim_inf(r_eye, anim_h_cb, EYE_R*2-10, EYE_R*2+10, 200, 200);
    make_anim_inf(l_pupil, anim_w_cb, PUPIL_R*2-8, PUPIL_R*2+8, 200, 200);
    make_anim_inf(r_pupil, anim_w_cb, PUPIL_R*2-8, PUPIL_R*2+8, 200, 200);
    lv_obj_set_height(l_lid_top, 4);
    lv_obj_set_height(r_lid_top, 4);
}

// ═══════════════════════════════════════════════════════════
//  状态切换主函数
// ═══════════════════════════════════════════════════════════

static void update_state_display(lcd_state_t state)
{
    stop_all_anims();
    hide_decorations();
    reset_lids();
    reset_pupils();

    // 恢复眼睛默认尺寸和形状
    lv_obj_set_size(l_eye, EYE_R*2, EYE_R*2);
    lv_obj_set_size(r_eye, EYE_R*2, EYE_R*2);
    lv_obj_set_style_radius(l_eye, EYE_R, 0);
    lv_obj_set_style_radius(r_eye, EYE_R, 0);
    lv_obj_set_style_bg_opa(l_eye, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(r_eye, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(l_pupil, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(r_pupil, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_opa(l_shine, LV_OPA_80, 0);
    lv_obj_set_style_bg_opa(r_shine, LV_OPA_80, 0);

    switch (state) {
    case LCD_STATE_SLEEPING:     expr_sleeping();     break;
    case LCD_STATE_CONNECTING:   expr_connecting();   break;
    case LCD_STATE_CONNECTED:    expr_connected();    break;
    case LCD_STATE_ERROR:        expr_error();        break;
    case LCD_STATE_THINKING:     expr_thinking();     break;
    case LCD_STATE_SPEAKING:     expr_speaking();     break;
    case LCD_STATE_LISTENING:    expr_listening();    break;
    case LCD_STATE_HAPPY:        expr_happy();        break;
    case LCD_STATE_DAYDREAM:     expr_daydream();     break;
    case LCD_STATE_IN_LOVE:      expr_in_love();      break;
    case LCD_STATE_EATING:       expr_eating();       break;
    case LCD_STATE_WORKOUT:      expr_workout();      break;
    case LCD_STATE_STUDYING:     expr_studying();     break;
    case LCD_STATE_WATCHING_TV:  expr_watching_tv();  break;
    case LCD_STATE_IGNORING:     expr_ignoring();     break;
    case LCD_STATE_ANGRY:        expr_angry();        break;
    case LCD_STATE_SURPRISED:    expr_surprised();    break;
    case LCD_STATE_BORED:        expr_bored();        break;
    case LCD_STATE_CYBER:        expr_cyber();        break;
    case LCD_STATE_DIZZY:        expr_dizzy();        break;
    case LCD_STATE_SHY:          expr_shy();          break;
    case LCD_STATE_EXCITED:      expr_excited();      break;
    default: break;
    }

    // 更新状态文字
    if (status_label && state < LCD_STATE_COUNT) {
        lv_label_set_text(status_label, state_labels[state]);
    }

    // WiFi 图标颜色
    if (wifi_icon) {
        lv_color_t wc;
        if (state == LCD_STATE_ERROR)      wc = lv_color_hex(0xFF4040);
        else if (state == LCD_STATE_CONNECTING) wc = lv_color_hex(0xFFAA00);
        else if (state == LCD_STATE_CONNECTED || state == LCD_STATE_SLEEPING)
                                           wc = lv_color_hex(0x4080A0);
        else                               wc = lv_color_hex(0x00B4D8);
        lv_obj_set_style_text_color(wifi_icon, wc, 0);
    }
}

// ═══════════════════════════════════════════════════════════
//  随机情绪触发（在 CONNECTED/SLEEPING 状态下随机插入）
// ═══════════════════════════════════════════════════════════

// 可随机触发的情绪状态池
static const lcd_state_t mood_pool[] = {
    LCD_STATE_HAPPY,
    LCD_STATE_DAYDREAM,
    LCD_STATE_IN_LOVE,
    LCD_STATE_EATING,
    LCD_STATE_BORED,
    LCD_STATE_SHY,
    LCD_STATE_EXCITED,
    LCD_STATE_WATCHING_TV,
    LCD_STATE_DIZZY,
    LCD_STATE_STUDYING,
};
#define MOOD_POOL_SIZE (sizeof(mood_pool)/sizeof(mood_pool[0]))

static void mood_timer_cb(lv_timer_t *t)
{
    (void)t;
    // 只在空闲状态下触发随机情绪
    if (current_state != LCD_STATE_CONNECTED &&
        current_state != LCD_STATE_SLEEPING) return;

    mood_timer_ticks++;
    // 每 ~30s 有 30% 概率触发一次随机情绪，持续 4s 后恢复
    if (mood_timer_ticks < 30) return;
    mood_timer_ticks = 0;

    uint32_t r = (uint32_t)(esp_timer_get_time() & 0xFFFF);
    if ((r % 10) >= 3) return;  // 70% 概率跳过

    lcd_state_t mood = mood_pool[r % MOOD_POOL_SIZE];
    lcd_state_t prev = current_state;

    if (lvgl_port_lock(0)) {
        update_state_display(mood);
        lvgl_port_unlock();
    }

    // 4 秒后恢复
    vTaskDelay(pdMS_TO_TICKS(4000));

    if (lvgl_port_lock(0)) {
        current_state = prev;
        update_state_display(prev);
        lvgl_port_unlock();
    }
}

// ═══════════════════════════════════════════════════════════
//  UI 初始化
// ═══════════════════════════════════════════════════════════

static void add_text_to_mouth(const char *prefix, const char *content, lv_color_t color)
{
    lv_obj_t *line = lv_label_create(mouth_area);
    lv_obj_set_style_text_font(line, font_text, 0);
    lv_obj_set_style_text_color(line, color, 0);
    lv_obj_set_width(line, LCD_H_RES - 40);
    lv_label_set_long_mode(line, LV_LABEL_LONG_WRAP);

    char buf[512];
    if (prefix && prefix[0]) {
        snprintf(buf, sizeof(buf), "%s%s", prefix, content);
    } else {
        snprintf(buf, sizeof(buf), "%s", content);
    }
    lv_label_set_text(line, buf);
    lv_obj_scroll_to_view_recursive(line, LV_ANIM_ON);

    uint32_t child_cnt = lv_obj_get_child_cnt(mouth_area);
    if (child_cnt > 20) {
        lv_obj_t *first = lv_obj_get_child(mouth_area, 0);
        if (first) lv_obj_delete(first);
    }
}

static void setup_ui(void)
{
    font_text  = &font_chinese_14;
    font_small = &lv_font_montserrat_12;

    screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    // ── 脸部区域（上半部分，放两个眼睛） ──────────────────
    face_area = lv_obj_create(screen);
    lv_obj_set_size(face_area, LCD_H_RES, 130);
    lv_obj_align(face_area, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(face_area, COLOR_FACE_BG, 0);
    lv_obj_set_style_bg_opa(face_area, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(face_area, 0, 0);
    lv_obj_set_style_border_width(face_area, 0, 0);
    lv_obj_set_style_pad_all(face_area, 0, 0);
    lv_obj_set_scrollbar_mode(face_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(face_area, LV_OBJ_FLAG_SCROLLABLE);

    // 两个眼睛，间距 50px
    create_eye(true,  -55);   // 左眼
    create_eye(false,  55);   // 右眼

    // ── 对话区域 ──────────────────────────────────────────
    mouth_area = lv_obj_create(screen);
    lv_obj_set_size(mouth_area, LCD_H_RES - 20, LCD_V_RES - 130 - 28);
    lv_obj_align(mouth_area, LV_ALIGN_TOP_MID, 0, 134);
    lv_obj_set_style_bg_color(mouth_area, COLOR_MOUTH_BG, 0);
    lv_obj_set_style_bg_opa(mouth_area, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(mouth_area, 10, 0);
    lv_obj_set_style_border_width(mouth_area, 1, 0);
    lv_obj_set_style_border_color(mouth_area, COLOR_MOUTH_BORDER, 0);
    lv_obj_set_style_border_opa(mouth_area, LV_OPA_60, 0);
    lv_obj_set_style_pad_all(mouth_area, 6, 0);
    lv_obj_set_scrollbar_mode(mouth_area, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(mouth_area, LV_DIR_VER);
    lv_obj_set_flex_flow(mouth_area, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mouth_area, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(mouth_area, 3, 0);

    mouth_label = lv_label_create(mouth_area);
    lv_obj_set_style_text_font(mouth_label, font_text, 0);
    lv_obj_set_style_text_color(mouth_label, COLOR_TEXT_DIM, 0);
    lv_label_set_text(mouth_label, "");
    lv_obj_set_width(mouth_label, LCD_H_RES - 40);
    lv_label_set_long_mode(mouth_label, LV_LABEL_LONG_WRAP);

    // ── 底部状态栏 ────────────────────────────────────────
    // 状态文字（WiFi 图标左边）
    status_label = lv_label_create(screen);
    lv_obj_set_style_text_font(status_label, font_small, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x80A0C0), 0);
    lv_label_set_text(status_label, "在睡觉");
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_RIGHT, -36, -6);

    // WiFi 图标（右下角）
    wifi_icon = lv_label_create(screen);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_icon, COLOR_WIFI, 0);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_align(wifi_icon, LV_ALIGN_BOTTOM_RIGHT, -6, -6);

    // 定时眨眼（每 4 秒）
    lv_timer_create(blink_timer_cb, 4000, NULL);
    // 随机情绪（每 1 秒 tick）
    lv_timer_create(mood_timer_cb, 1000, NULL);
}

// ═══════════════════════════════════════════════════════════
//  硬件初始化
// ═══════════════════════════════════════════════════════════

esp_err_t lcd_display_init(void)
{
    ESP_LOGI(TAG, "Initializing LCD with LVGL (ST7789 240x240)");

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << PIN_BL
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    lcd_backlight_set(false);

    spi_bus_config_t buscfg = {
        .sclk_io_num = PIN_SCLK,
        .mosi_io_num = PIN_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_DC,
        .cs_gpio_num = PIN_CS,
        .pclk_hz = LCD_PIXEL_CLK,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 3,
        .trans_queue_depth = 10,
    };
    ret = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &lcd_io);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Panel IO failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(lcd_io, &panel_config, &lcd_panel));
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

    ESP_LOGI(TAG, "Initializing LVGL...");
    lv_init();

    const lvgl_port_cfg_t port_cfg = {
        .task_priority     = 4,
        .task_stack        = 8192,
        .task_affinity     = 1,
        .task_max_sleep_ms = 500,
        .timer_period_ms   = 5,
    };
    ret = lvgl_port_init(&port_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL port init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle     = lcd_io,
        .panel_handle  = lcd_panel,
        .control_handle = NULL,
        .buffer_size   = LCD_H_RES * 20,
        .double_buffer = false,
        .trans_size    = 0,
        .hres          = LCD_H_RES,
        .vres          = LCD_V_RES,
        .monochrome    = false,
        .rotation = { .swap_xy = false, .mirror_x = false, .mirror_y = false },
        .color_format  = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma    = 1,
            .buff_spiram = 0,
            .sw_rotate   = 0,
            .swap_bytes  = 1,
            .full_refresh = 0,
            .direct_mode  = 0,
        },
    };

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    if (lvgl_disp == NULL) {
        ESP_LOGE(TAG, "Failed to add LVGL display");
        return ESP_FAIL;
    }

    lcd_backlight_set(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "Setting up UI...");
    if (lvgl_port_lock(0)) {
        setup_ui();
        update_state_display(LCD_STATE_SLEEPING);
        lvgl_port_unlock();
    }

    ESP_LOGI(TAG, "LCD+LVGL initialized successfully");
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
        lv_color_t color = is_user ? COLOR_TEXT_USER : COLOR_TEXT_ASSIST;
        const char *prefix = is_user ? "> " : "";
        add_text_to_mouth(prefix, content, color);
        lvgl_port_unlock();
    }
}

void lcd_clear_chat(void)
{
    if (lvgl_port_lock(500)) {
        lv_obj_clean(mouth_area);
        mouth_label = lv_label_create(mouth_area);
        lv_obj_set_style_text_font(mouth_label, font_text, 0);
        lv_obj_set_style_text_color(mouth_label, COLOR_TEXT_DIM, 0);
        lv_label_set_text(mouth_label, "");
        lv_obj_set_width(mouth_label, LCD_H_RES - 40);
        lv_label_set_long_mode(mouth_label, LV_LABEL_LONG_WRAP);
        lvgl_port_unlock();
    }
}

// 该接口保留但不再显示在屏幕顶部（改显示在状态标签）
void lcd_set_status_text(const char *text)
{
    if (!text) return;
    if (lvgl_port_lock(500)) {
        if (status_label) {
            lv_label_set_text(status_label, text);
        }
        lvgl_port_unlock();
    }
}
