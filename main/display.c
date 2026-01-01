#include "display.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "app_state.h"
#include "touch_cst816s.h"
#include "web.h"
#include "safe_ota.h"
#include "can_gbt.h"
#include "sm_relay.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "driver/temperature_sensor.h"

#if CUBE_LCD_USE_ST7789
/* ST7789 via generic vendor panel in IDF 5.0 */
#include "esp_lcd_panel_vendor.h"
#else
#include "esp_lcd_ili9341.h"
#endif

static const char *TAG = "DISPLAY";
extern const lv_font_t lv_font_ru_14;
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_panel_io;
static SemaphoreHandle_t s_lvgl_lock;
static lv_disp_t *s_display;
static lv_color_t *s_buf1;
static lv_color_t *s_buf2;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_touch_drv;
static esp_timer_handle_t s_tick_timer;
static temperature_sensor_handle_t s_temp;
static int s_brightness = CUBE_LCD_BL_PWM_DUTY_DEFAULT;

/* UI */
static lv_obj_t *s_scr_chg, *s_scr_ses, *s_scr_chg, *s_scr_sys, *s_scr_wifi, *s_scr_log;
static lv_obj_t *s_ses_kwh, *s_ses_soc, *s_ses_min, *s_ses_req, *s_log_lbl, *s_can_lbl;
static int s_pending_boot;
static int s_edge = 0;
static lv_obj_t *s_voltage, *s_current, *s_power, *s_stage, *s_status;
static lv_obj_t *s_soc, *s_relay, *s_cml, *s_can;
static lv_obj_t *s_sys_temp, *s_sys_cpu, *s_sys_load, *s_sys_heap, *s_sys_wifi, *s_sys_touch;
static lv_obj_t *s_br_label;
static lv_obj_t *s_pad;
static lv_obj_t *s_wifi_ta;
static lv_obj_t *s_scan_lbl;
static lv_obj_t *s_list;
static lv_obj_t *s_ip_lbl;
static int s_page;
static int s_pick;

static bool lcd_flush_done(esp_lcd_panel_io_handle_t panel_io,
                           esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)panel_io; (void)edata;
    if (!s_display) return false;
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
    if (drv) lv_disp_flush_ready(drv);
    return false;
}

static void lv_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)drv;
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1,
                                               area->x2 + 1, area->y2 + 1, color_p);
    if (err != ESP_OK && s_display) lv_disp_flush_ready(s_display->driver);
}

static void tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(CUBE_LVGL_TICK_MS);
}

bool display_lock(void)
{
    return s_lvgl_lock && xSemaphoreTakeRecursive(s_lvgl_lock, pdMS_TO_TICKS(200)) == pdTRUE;
}
void display_unlock(void)
{
    if (s_lvgl_lock) xSemaphoreGiveRecursive(s_lvgl_lock);
}

void display_set_brightness(int percent)
{
    if (percent < 5) percent = 5;
    if (percent > 100) percent = 100;
    s_brightness = percent;
#if CUBE_LCD_BL_PWM_GPIO >= 0
    uint32_t duty = (uint32_t)((percent * 1023) / 100);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
#endif
}

int display_get_brightness(void) { return s_brightness; }

static void bl_init(void)
{
#if CUBE_LCD_BL_GPIO >= 0
    ledc_timer_config_t t = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CUBE_LCD_BL_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&t);
    ledc_channel_config_t c = {
        .gpio_num = CUBE_LCD_BL_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .timer_sel = LEDC_TIMER_0,
        .duty = (CUBE_LCD_BL_PWM_DUTY_DEFAULT * 1023) / 100,
        .hpoint = 0,
    };
    ledc_channel_config(&c);
    ESP_LOGW(TAG, "backlight PWM GPIO%d %d%%", CUBE_LCD_BL_PWM_GPIO, CUBE_LCD_BL_PWM_DUTY_DEFAULT);
#endif
}

static float read_temp_c(void)
{
    float t = 0;
    if (s_temp && temperature_sensor_get_celsius(s_temp, &t) == ESP_OK) return t;
    return -99.0f;
}

static int cpu_load_pct(void)
{
    /* Rough load from free heap pressure + task — simple estimate */
    static uint32_t last_idle;
    (void)last_idle;
    size_t free_now = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t total = free_now + heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
    if (total < 1) total = 1;
    int used = (int)(100 - (free_now * 100 / (total + free_now)));
    if (used < 0) used = 0;
    if (used > 99) used = 99;
    /* Prefer runtime stats if available — fallback: invert free heap ratio */
    UBaseType_t n = uxTaskGetNumberOfTasks();
    int load = 15 + (int)n * 3;
    if (load > 95) load = 95;
    return load;
}



static lv_obj_t *s_card_u;
static lv_obj_t *s_card_i;
static lv_obj_t *s_dots;
static void gesture_cb(lv_event_t *e);
static void apply_field(void);
static void refresh_scan_label(void);
static void resize_edges(void);
static void stop_hold_cb(lv_event_t *e);

static void style_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x071018), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xE8EEF4), 0);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scr, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_text_font(scr, &lv_font_ru_14, 0);
    }

static void apply_page(int p)
{
    lv_obj_t *pages[6];
    int prev = s_page;
    if (p < 0) p = 5;
    if (p > 5) p = 0;
    pages[0] = s_scr_chg; pages[1] = s_scr_ses; pages[2] = s_scr_chg;
    pages[3] = s_scr_wifi; pages[4] = s_scr_log; pages[5] = s_scr_sys;
    s_page = p;
    if (s_pad) lv_obj_add_flag(s_pad, LV_OBJ_FLAG_HIDDEN);
    if (pages[p]) {
        lv_scr_load(pages[p]);
        if (p == 3) {
            if (s_scan_lbl) lv_label_set_text(s_scan_lbl, "SCAN...");
            /* scan off: AP only */
            refresh_scan_label();
        }
    }
    (void)prev;
}

static lv_point_t s_down;
static bool s_arm;
static void gesture_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_indev_t *in = lv_indev_get_act();
    if (!in) return;
    lv_point_t pt;
    lv_indev_get_point(in, &pt);
    if (code == LV_EVENT_PRESSED) { s_down = pt; s_arm = true; return; }
    if (code != LV_EVENT_RELEASED || !s_arm) return;
    s_arm = false;
    int dx = pt.x - s_down.x;
    int dy = pt.y - s_down.y;
    (void)dx; (void)dy; (void)pt;
    /* Vertical scroll only. No page swipe. */
    return;
}

static int s_unlocked;
static int s_arm_act; /* 1 relay, 2 stop */
static lv_obj_t *s_req_v;
static lv_obj_t *s_req_i;
static lv_obj_t *s_time;
static lv_obj_t *s_out_v;
static lv_obj_t *s_out_a;
static lv_obj_t *s_watt;
static lv_obj_t *s_stat;
static void pad_show(void);
static lv_obj_t *s_active_ta;
static lv_obj_t *s_pass_box;
static lv_obj_t *s_pass_title;

static void ask_pass(int act)
{
    s_arm_act = act;
    if (s_pass_title) lv_label_set_text(s_pass_title, "пароль");
    if (s_wifi_ta) lv_textarea_set_text(s_wifi_ta, "");
    s_active_ta = s_wifi_ta;
    if (s_pass_box) lv_obj_add_flag(s_pass_box, LV_OBJ_FLAG_HIDDEN);
    pad_show();
}

static void stop_hold_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
    if (!s_unlocked) { ask_pass(2); return; }
    s_unlocked = 0;
    sm_stop_session();
}

static void relay_hold_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) return;
    if (!s_unlocked) { ask_pass(1); return; }
    s_unlocked = 0;
    sm_request_relay_test();
}

static void session_go_cb(lv_event_t *e)
{
    (void)e;
    sm_start_session();
    /* auto: no start button required */
}

static void br_cb(lv_event_t *e)
{
    int v = lv_slider_get_value(lv_event_get_target(e));
    display_set_brightness(v);
    if (s_br_label) {
        char b[24];
        snprintf(b, sizeof(b), "Bright %d%%", v);
        lv_label_set_text(s_br_label, b);
    }
}

static void pad_hide(void)
{
    if (s_pad) lv_obj_add_flag(s_pad, LV_OBJ_FLAG_HIDDEN);
}

static void pad_show(void)
{
    if (s_pad) lv_obj_clear_flag(s_pad, LV_OBJ_FLAG_HIDDEN);
}

static int s_set_v = 240;
static int s_set_i = 50;
static int s_set_soc = 20;
static float s_cap_kwh = 7.5f;
static lv_obj_t *s_set_lbl;
static lv_obj_t *s_vin_ta;
static lv_obj_t *s_v_ta;
static lv_obj_t *s_i_ta;
static lv_obj_t *s_soc_ta;
static lv_obj_t *s_kwh_ta;
static lv_obj_t *s_ip_box;
static lv_obj_t *s_ip_msg;
static int s_ip_shown;
static lv_obj_t *s_ota_box;
static lv_obj_t *s_ota_bar;
static lv_obj_t *s_ota_txt;
static lv_obj_t *s_edge_lbl;

static void pad_key_cb(lv_event_t *e)
{
    const char *k = (const char *)lv_event_get_user_data(e);
    lv_obj_t *ta = s_active_ta ? s_active_ta : s_wifi_ta;
    if (!ta || !k) return;
    if (strcmp(k, "<") == 0) { lv_textarea_del_char(ta); return; }
    if (strcmp(k, "C") == 0) { lv_textarea_set_text(ta, ""); return; }
    if (strcmp(k, "OK") == 0) {
        if (s_active_ta == s_wifi_ta) {
            if (s_wifi_ta && strcmp(lv_textarea_get_text(s_wifi_ta), "1234") == 0) s_unlocked = 1;
            pad_hide();
            if (s_pass_box) lv_obj_add_flag(s_pass_box, LV_OBJ_FLAG_HIDDEN);
            return;
        }
        apply_field(); pad_hide(); return;
    }
    if (k[0] < '0' || k[0] > '9') return;
    lv_textarea_add_text(ta, k);
}

static void ta_focus_cb(lv_event_t *e)
{
    s_active_ta = lv_event_get_target(e);
    if (!s_pad) return;
    lv_obj_clear_flag(s_pad, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_pad);
}

static void relay_test_cb(lv_event_t *e)
{
    (void)e;
    sm_request_relay_test();
}

static void wifi_rst_cb(lv_event_t *e)
{
    (void)e;
    web_wifi_restart();
}

static void pass_close_cb(lv_event_t *e)
{
    (void)e;
    pad_hide();
    if (s_pass_box) lv_obj_add_flag(s_pass_box, LV_OBJ_FLAG_HIDDEN);
    if (s_ip_box) lv_obj_add_flag(s_ip_box, LV_OBJ_FLAG_HIDDEN);
}

static void show_pass(const char *ssid)
{
    if (s_pass_title) lv_label_set_text(s_pass_title, ssid && ssid[0] ? ssid : "network");
    if (s_wifi_ta) lv_textarea_set_text(s_wifi_ta, "");
    s_active_ta = s_wifi_ta;
    if (s_pass_box) {
        lv_obj_clear_flag(s_pass_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_pass_box);
    }
    pad_show();
}

static void net_pick_cb(lv_event_t *e)
{
    s_pick = (int)(intptr_t)lv_event_get_user_data(e);
    show_pass(web_scan_ssid(s_pick));
}
static void refresh_scan_label(void)
{
    int n = web_scan_count();
    if (s_scan_lbl) lv_label_set_text(s_scan_lbl, n ? "tap a network" : "nets: 0");
    if (!s_list) return;
    lv_obj_clean(s_list);
    for (int i = 0; i < n && i < 12; ++i) {
        lv_obj_t *b = lv_btn_create(s_list);
        lv_obj_set_width(b, 200);
        lv_obj_set_height(b, 28);
        lv_obj_set_style_bg_color(b, lv_color_hex(i == s_pick ? 0x1F6F5B : 0x1A2430), 0);
        lv_obj_add_event_cb(b, net_pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_set_pos(b, 4, i * 34);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, web_scan_ssid(i));
        lv_obj_set_style_text_color(l, lv_color_hex(0xF4F7FB), 0);
        lv_obj_center(l);
    }
}

static void scan_cb(lv_event_t *e)
{
    (void)e;
    if (s_scan_lbl) lv_label_set_text(s_scan_lbl, "SCAN...");
    (void)web_scan_now();
    s_pick = 0;
    refresh_scan_label();
}

static void pick_cb(lv_event_t *e)
{
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    int n = web_scan_count();
    if (n <= 0) return;
    s_pick = (s_pick + d + n) % n;
    refresh_scan_label();
    if (s_wifi_ta) lv_obj_clear_flag(s_wifi_ta, LV_OBJ_FLAG_HIDDEN);
    pad_show();
}

static void join_cb(lv_event_t *e)
{
    (void)e;
    const char *ssid = web_scan_ssid(s_pick);
    const char *pass = s_wifi_ta ? lv_textarea_get_text(s_wifi_ta) : "";
    if (!ssid || !ssid[0]) {
        if (s_ip_lbl) lv_label_set_text(s_ip_lbl, "no scan");
        return;
    }
    (void)web_sta_join(ssid, pass);
    if (s_ip_lbl) lv_label_set_text(s_ip_lbl, "joining...");
    if (s_pass_title) lv_label_set_text(s_pass_title, "connecting...");
    pad_hide();
}

static lv_obj_t *s_edge_l;
static lv_obj_t *s_edge_r;

static void make_pad(lv_obj_t *parent)
{
    s_pad = lv_obj_create(parent);
    lv_obj_set_size(s_pad, 240, 280);
    lv_obj_align(s_pad, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(s_pad, lv_color_hex(0x1A1D22), 0);
    lv_obj_set_style_bg_opa(s_pad, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pad, 0, 0);
    lv_obj_set_style_pad_all(s_pad, 0, 0);
    lv_obj_clear_flag(s_pad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *banner = lv_obj_create(s_pad);
    lv_obj_set_size(banner, 220, 36);
    lv_obj_align(banner, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0x2EC8C0), 0);
    lv_obj_set_style_bg_opa(banner, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(banner, 8, 0);
    lv_obj_set_style_border_width(banner, 0, 0);
    lv_obj_clear_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *bt = lv_label_create(banner);
    lv_label_set_text(bt, "4 цифры");
    lv_obj_set_style_text_color(bt, lv_color_hex(0x07302E), 0);
    lv_obj_center(bt);
    const char *keys[] = {"1","2","3","4","5","6","7","8","9","<","0","OK"};
    for (int i = 0; i < 12; ++i) {
        lv_obj_t *b = lv_btn_create(s_pad);
        if (!b) break;
        lv_obj_set_size(b, 76, 52);
        lv_obj_set_pos(b, 4 + (i % 3) * 78, 52 + (i / 3) * 56);
        lv_obj_set_style_bg_color(b, lv_color_hex(0xC5C8CE), 0);
        lv_obj_set_style_radius(b, 0, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_add_event_cb(b, pad_key_cb, LV_EVENT_CLICKED, (void *)keys[i]);
        lv_obj_t *l = lv_label_create(b);
        if (!l) break;
        lv_label_set_text(l, keys[i][0] == 'O' ? "V" : keys[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(0x1A1D22), 0);
        lv_obj_center(l);
    }
    lv_obj_add_flag(s_pad, LV_OBJ_FLAG_HIDDEN);
}

static void resize_edges(void)
{
    if (s_edge_l) lv_obj_set_width(s_edge_l, s_edge);
    if (s_edge_r) lv_obj_set_width(s_edge_r, s_edge);
}

static void make_edges(lv_obj_t *parent)
{
    s_edge_l = lv_obj_create(parent);
    lv_obj_set_size(s_edge_l, s_edge, 280);
    lv_obj_align(s_edge_l, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_edge_l, lv_color_hex(0x1F6F5B), 0);
    lv_obj_set_style_bg_opa(s_edge_l, LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_edge_l, 0, 0);
    lv_obj_clear_flag(s_edge_l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_edge_l, gesture_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_edge_l, gesture_cb, LV_EVENT_RELEASED, NULL);
    s_edge_r = lv_obj_create(parent);
    lv_obj_set_size(s_edge_r, s_edge, 280);
    lv_obj_align(s_edge_r, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(s_edge_r, lv_color_hex(0x1F6F5B), 0);
    lv_obj_set_style_bg_opa(s_edge_r, LV_OPA_30, 0);
    lv_obj_set_style_border_width(s_edge_r, 0, 0);
    lv_obj_clear_flag(s_edge_r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_edge_r, gesture_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(s_edge_r, gesture_cb, LV_EVENT_RELEASED, NULL);
}

static lv_obj_t *card(lv_obj_t *parent, int x, int y, lv_obj_t **value, const char *cap)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, 104, 56);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_style_bg_color(c, lv_color_hex(0x122033), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_radius(c, 10, 0);
    lv_obj_set_style_pad_all(c, 6, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(c, gesture_cb, LV_EVENT_GESTURE, NULL);
    lv_obj_t *k = lv_label_create(c);
    lv_label_set_text(k, cap);
    lv_obj_set_style_text_color(k, lv_color_hex(0x8FB4D6), 0);
    lv_obj_align(k, LV_ALIGN_TOP_LEFT, 0, 0);
    *value = lv_label_create(c);
    lv_label_set_text(*value, "--");
    lv_obj_set_style_text_color(*value, lv_color_hex(0xF4F7FB), 0);
    lv_obj_set_width(*value, 92);
    lv_label_set_long_mode(*value, LV_LABEL_LONG_CLIP);
    lv_obj_align(*value, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    return c;
}

static lv_obj_t *line(lv_obj_t *parent, const char *txt, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    if (!l) {
        ESP_LOGE(TAG, "LVGL OOM label '%s'", txt ? txt : "");
        return NULL;
    }
    lv_label_set_text(l, txt);
    lv_obj_set_width(l, 216);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(l, lv_color_hex(0xD5DEE8), 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 12, y);
    return l;
}


static void apply_field(void)
{
    int v = s_v_ta ? atoi(lv_textarea_get_text(s_v_ta)) : s_set_v;
    int i = s_i_ta ? atoi(lv_textarea_get_text(s_i_ta)) : s_set_i;
    int soc = s_soc_ta ? atoi(lv_textarea_get_text(s_soc_ta)) : s_set_soc;
    float cap = s_kwh_ta ? (float)atof(lv_textarea_get_text(s_kwh_ta)) : s_cap_kwh;
    if (v < 50) v = 50;
    if (v > 500) v = 500;
    if (i < 1) i = 1;
    if (i > 250) i = 250;
    if (soc < 0) soc = 0;
    if (soc > 100) soc = 100;
    if (cap < 1.0f) cap = 1.0f;
    if (cap > 200.0f) cap = 200.0f;
    s_set_v = v; s_set_i = i; s_set_soc = soc; s_cap_kwh = cap;
    app_state_set_bcl((uint16_t)(v * 10), (int16_t)(i * 10), 1);
    app_state_set_battery_vi((uint16_t)(v * 10), (int16_t)(i * 10));
    app_state_set_soc((uint8_t)soc);
    app_state_set_cap_kwh(cap);
    if (s_set_lbl) {
        char b[48];
        snprintf(b, sizeof(b), "set %dV %dA SOC%d %.1fkWh", v, i, soc, (double)cap);
        lv_label_set_text(s_set_lbl, b);
    }
}

static void step_cb(lv_event_t *e)
{
    int id = (int)(intptr_t)lv_event_get_user_data(e);
    if (id == 1) s_set_v += 10;
    if (id == 2) s_set_v -= 10;
    if (id == 3) s_set_i += 5;
    if (id == 4) s_set_i -= 5;
    if (id == 5) s_cap_kwh += 0.5f;
    if (id == 6) s_cap_kwh -= 0.5f;
    if (id == 7) s_set_soc += 5;
    if (id == 8) s_set_soc -= 5;
    if (s_set_v < 50) s_set_v = 50;
    if (s_set_v > 500) s_set_v = 500;
    if (s_set_i < 5) s_set_i = 5;
    if (s_set_i > 250) s_set_i = 250;
    if (s_cap_kwh < 1.0f) s_cap_kwh = 1.0f;
    if (s_set_soc < 0) s_set_soc = 0;
    if (s_set_soc > 100) s_set_soc = 100;
    app_state_set_bcl((uint16_t)(s_set_v * 10), (int16_t)(s_set_i * 10), 1);
    app_state_set_battery_vi((uint16_t)(s_set_v * 10), (int16_t)(s_set_i * 10));
    app_state_set_cap_kwh(s_cap_kwh);
    app_state_set_soc((uint8_t)s_set_soc);
    if (s_vin_ta) app_state_set_vin(lv_textarea_get_text(s_vin_ta));
    if (s_set_lbl) {
        char b[72];
        snprintf(b, sizeof(b), "%dV %dA %.1fkWh SOC%d", s_set_v, s_set_i, (double)s_cap_kwh, s_set_soc);
        lv_label_set_text(s_set_lbl, b);
    }
}

static void proto_cb(lv_event_t *e)
{
    app_state_set_protocol((uint8_t)(intptr_t)lv_event_get_user_data(e));
}

static void edge_cb(lv_event_t *e)
{
    s_edge += (int)(intptr_t)lv_event_get_user_data(e);
    if (s_edge < 8) s_edge = 8;
    if (s_edge > 80) s_edge = 80;
    if (s_edge_lbl) {
        char b[24];
        snprintf(b, sizeof(b), "swipe %d px", s_edge);
        lv_label_set_text(s_edge_lbl, b);
    }
    resize_edges();
}

static void boot_old_cb(lv_event_t *e)
{
    (void)e;
    display_ota_progress(100, "OLD FW");
    s_pending_boot = 1;
}

static void build_ui(void)
{
    s_scr_chg = lv_scr_act();
    style_screen(s_scr_chg);
    line(s_scr_chg, "КУБ 2.7.8", 4);
    s_time = line(s_scr_chg, "время 0 мин", 28);
    s_relay = line(s_scr_chg, "выход OFF", 50);
    s_req_v = line(s_scr_chg, "запрос 240 В", 78);
    s_req_i = line(s_scr_chg, "запрос 50 А", 100);
    s_watt = line(s_scr_chg, "мощность -- Вт", 122);
    s_out_v = line(s_scr_chg, "выход -- В", 150);
    s_out_a = line(s_scr_chg, "выход -- А", 172);
    s_stat = line(s_scr_chg, "статус 0", 194);
    s_v_ta = lv_textarea_create(s_scr_chg);
    lv_obj_set_size(s_v_ta, 90, 28); lv_obj_set_pos(s_v_ta, 12, 220);
    lv_textarea_set_one_line(s_v_ta, true); lv_textarea_set_text(s_v_ta, "240");
    lv_obj_add_event_cb(s_v_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);
    s_i_ta = lv_textarea_create(s_scr_chg);
    lv_obj_set_size(s_i_ta, 90, 28); lv_obj_set_pos(s_i_ta, 120, 220);
    lv_textarea_set_one_line(s_i_ta, true); lv_textarea_set_text(s_i_ta, "50");
    lv_obj_add_event_cb(s_i_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_watt, stop_hold_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_flag(s_watt, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_relay, relay_hold_cb, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_add_flag(s_relay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_scr_chg, LV_OBJ_FLAG_SCROLLABLE);
    s_dots = line(s_scr_chg, "ниже: точка, лог, оригинал", 250);
    s_ip_lbl = line(s_scr_chg, "Cube 192.168.4.1", 274);
    s_log_lbl = line(s_scr_chg, "лог", 298);
    lv_obj_set_width(s_log_lbl, 216);
    lv_label_set_long_mode(s_log_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_t *orig = lv_btn_create(s_scr_chg);
    lv_obj_set_size(orig, 200, 36);
    lv_obj_set_pos(orig, 12, 430);
    lv_obj_add_event_cb(orig, boot_old_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *orig_l = lv_label_create(orig);
    lv_label_set_text(orig_l, "оригинал");
    lv_obj_center(orig_l);
    s_status = line(s_scr_chg, "3 включения = слот", 476);


    s_scr_ses = lv_obj_create(NULL);
    style_screen(s_scr_ses);
    lv_obj_add_flag(s_scr_ses, LV_OBJ_FLAG_SCROLLABLE);
    line(s_scr_ses, "СЕССИЯ", 8);
    s_ses_min = line(s_scr_ses, "время --", 36);
    s_ses_kwh = line(s_scr_ses, "кВтч --", 58);
    s_ses_soc = line(s_scr_ses, "оценка от 7.5", 80);
    s_ses_req = line(s_scr_ses, "запрос --", 104);
    lv_obj_t *go = lv_btn_create(s_scr_ses);
    lv_obj_set_size(go, 200, 36);
    lv_obj_align(go, LV_ALIGN_TOP_MID, 0, 140);
    lv_obj_set_style_bg_color(go, lv_color_hex(0x1F6F5B), 0);
    lv_obj_add_event_cb(go, session_go_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *gl = lv_label_create(go); lv_label_set_text(gl, "START"); lv_obj_center(gl);
    s_scr_chg = lv_obj_create(NULL);
    style_screen(s_scr_chg);
    lv_obj_add_flag(s_scr_chg, LV_OBJ_FLAG_SCROLLABLE);
    line(s_scr_chg, "ЗАРЯДКА", 300);
    s_set_lbl = line(s_scr_chg, "поле, цифры, OK", 320);
    line(s_scr_chg, "V", 342);
    s_v_ta = lv_textarea_create(s_scr_chg);
    lv_obj_set_size(s_v_ta, 120, 32);
    lv_obj_set_pos(s_v_ta, 70, 338);
    lv_textarea_set_one_line(s_v_ta, true);
    lv_textarea_set_max_length(s_v_ta, 4);
    lv_textarea_set_text(s_v_ta, "240");
    lv_obj_add_event_cb(s_v_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);
    line(s_scr_chg, "A", 380);
    s_i_ta = lv_textarea_create(s_scr_chg);
    lv_obj_set_size(s_i_ta, 120, 32);
    lv_obj_set_pos(s_i_ta, 70, 376);
    lv_textarea_set_one_line(s_i_ta, true);
    lv_textarea_set_max_length(s_i_ta, 4);
    lv_textarea_set_text(s_i_ta, "50");
    lv_obj_add_event_cb(s_i_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);
    line(s_scr_chg, "SOC", 418);
    s_soc_ta = lv_textarea_create(s_scr_chg);
    lv_obj_set_size(s_soc_ta, 120, 32);
    lv_obj_set_pos(s_soc_ta, 70, 414);
    lv_textarea_set_one_line(s_soc_ta, true);
    lv_textarea_set_max_length(s_soc_ta, 3);
    lv_textarea_set_text(s_soc_ta, "20");
    lv_obj_add_event_cb(s_soc_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);
    line(s_scr_chg, "kWh", 456);
    s_kwh_ta = lv_textarea_create(s_scr_chg);
    lv_obj_set_size(s_kwh_ta, 120, 32);
    lv_obj_set_pos(s_kwh_ta, 70, 452);
    lv_textarea_set_one_line(s_kwh_ta, true);
    lv_textarea_set_max_length(s_kwh_ta, 5);
    lv_textarea_set_text(s_kwh_ta, "7.5");
    lv_obj_add_event_cb(s_kwh_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);
    s_active_ta = s_v_ta;
    {
        const char *pn[] = {"авто", "2015", "2011"};
        const int pv[] = {0, 1, 2};
        for (int i = 0; i < 3; ++i) {
            lv_obj_t *b = lv_btn_create(s_scr_chg);
            lv_obj_set_size(b, 70, 28);
            lv_obj_set_pos(b, 8 + i * 76, 500);
            lv_obj_set_style_shadow_width(b, 0, 0);
            lv_obj_add_event_cb(b, proto_cb, LV_EVENT_CLICKED, (void *)(intptr_t)pv[i]);
            lv_obj_t *l = lv_label_create(b);
            lv_label_set_text(l, pn[i]);
            lv_obj_center(l);
        }
    }
    s_scr_log = lv_obj_create(NULL);
    style_screen(s_scr_log);
    lv_obj_add_flag(s_scr_log, LV_OBJ_FLAG_SCROLLABLE);
    line(s_scr_log, "ЛОГИ ESP", 8);
    lv_obj_add_flag(s_scr_log, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_scr_log, LV_DIR_VER);
    s_log_lbl = line(s_scr_chg, "нет событий", 760);
    lv_label_set_long_mode(s_log_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_log_lbl, 210);
    lv_obj_set_style_text_font(s_log_lbl, &lv_font_montserrat_14, 0);
    line(s_scr_log, "ЛОГИ CAN", 136);
    s_can_lbl = line(s_scr_log, "нет кадров", 156);
    lv_label_set_long_mode(s_can_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_height(s_can_lbl, 110);
    s_scr_sys = lv_obj_create(NULL);
    style_screen(s_scr_sys);
    line(s_scr_sys, "СИСТЕМА 2.7.8", 4);
    s_sys_temp = line(s_scr_sys, "CPU -- C", 28);
    s_sys_cpu = line(s_scr_sys, "F 160 MHz", 48);
    s_sys_load = line(s_scr_sys, "Load -- %", 68);
    s_sys_heap = line(s_scr_sys, "Heap --", 88);
    s_sys_wifi = line(s_scr_sys, "AP Cube", 108);
    s_sys_touch = line(s_scr_sys, "Touch --", 128);
    s_br_label = line(s_scr_sys, "Bright 70%", 152);
    lv_obj_t *sl = lv_slider_create(s_scr_sys);
    lv_obj_set_width(sl, 210);
    lv_slider_set_range(sl, 5, 100);
    lv_slider_set_value(sl, s_brightness, LV_ANIM_OFF);
    lv_obj_align(sl, LV_ALIGN_TOP_MID, 0, 176);
    lv_obj_add_event_cb(sl, br_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *rb = lv_btn_create(s_scr_sys);
    lv_obj_set_size(rb, 180, 36);
    lv_obj_align(rb, LV_ALIGN_TOP_MID, 0, 210);
    lv_obj_set_style_bg_color(rb, lv_color_hex(0x1F6FEB), 0);
    lv_obj_set_style_shadow_width(rb, 0, 0);
    lv_obj_add_event_cb(rb, wifi_rst_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rl = lv_label_create(rb);
    lv_label_set_text(rl, "RESTART WiFi");
    lv_obj_center(rl);
    lv_obj_t *tb = lv_btn_create(s_scr_sys);
    lv_obj_set_size(tb, 180, 32);
    lv_obj_align(tb, LV_ALIGN_TOP_MID, 0, 248);
    lv_obj_set_style_bg_color(tb, lv_color_hex(0xC45C26), 0);
    lv_obj_set_style_shadow_width(tb, 0, 0);
    lv_obj_add_event_cb(tb, relay_test_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *tbl = lv_label_create(tb);
    lv_label_set_text(tbl, "РЕЛЕ");
    lv_obj_center(tbl);
    lv_obj_add_flag(s_scr_sys, LV_OBJ_FLAG_SCROLLABLE);
    s_edge_lbl = line(s_scr_sys, "swipe 28 px", 286);
    lv_obj_t *em = lv_btn_create(s_scr_sys);
    lv_obj_set_size(em, 70, 28);
    lv_obj_set_pos(em, 12, 310);
    lv_obj_add_event_cb(em, edge_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-8);
    lv_obj_t *eml = lv_label_create(em); lv_label_set_text(eml, "край-"); lv_obj_center(eml);
    lv_obj_t *ep = lv_btn_create(s_scr_sys);
    lv_obj_set_size(ep, 70, 28);
    lv_obj_set_pos(ep, 90, 310);
    lv_obj_add_event_cb(ep, edge_cb, LV_EVENT_CLICKED, (void *)(intptr_t)8);
    lv_obj_t *epl = lv_label_create(ep); lv_label_set_text(epl, "край+"); lv_obj_center(epl);
    lv_obj_t *ob = lv_btn_create(s_scr_sys);
    lv_obj_set_size(ob, 160, 28);
    lv_obj_set_pos(ob, 12, 346);
    lv_obj_add_event_cb(ob, boot_old_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *obl = lv_label_create(ob); lv_label_set_text(obl, "ОРИГИНАЛ"); /* back: 3 power cycles */ lv_obj_center(obl);
    line(s_scr_sys, "OTA с сайта = полоса", 382);


    s_scr_wifi = lv_obj_create(NULL);
    style_screen(s_scr_wifi);
    line(s_scr_wifi, "СЕТЬ", 2);
    lv_obj_add_flag(s_scr_wifi, LV_OBJ_FLAG_SCROLLABLE);
    s_list = lv_obj_create(s_scr_chg);
    lv_obj_set_size(s_list, 220, 160);
    lv_obj_set_pos(s_list, 10, 560);
    lv_obj_add_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_row(s_list, 4, 0);
    s_scan_lbl = line(s_scr_chg, "nets: --", 540);
    s_ip_lbl = line(s_scr_chg, "AP 192.168.4.1", 730);

    lv_obj_t *sc = lv_btn_create(s_scr_wifi);
    lv_obj_set_size(sc, 70, 32);
    lv_obj_align(sc, LV_ALIGN_TOP_LEFT, 6, 258);
    lv_obj_set_style_shadow_width(sc, 0, 0);
    lv_obj_add_event_cb(sc, scan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *slb = lv_label_create(sc); lv_label_set_text(slb, "SCAN"); lv_obj_center(slb);

    line(s_scr_wifi, "tap net, digits, CONNECT", 248);

    lv_obj_set_style_text_font(lv_layer_top(), &lv_font_ru_14, 0);
    make_edges(lv_layer_top());
    s_pass_box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_pass_box, 228, 118);
    lv_obj_align(s_pass_box, LV_ALIGN_TOP_MID, 0, 8);
    lv_obj_set_style_bg_color(s_pass_box, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(s_pass_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_pass_box, lv_color_hex(0x1F6F5B), 0);
    lv_obj_set_style_border_width(s_pass_box, 2, 0);
    lv_obj_clear_flag(s_pass_box, LV_OBJ_FLAG_SCROLLABLE);
    s_pass_title = lv_label_create(s_pass_box);
    lv_label_set_text(s_pass_title, "password");
    lv_obj_align(s_pass_title, LV_ALIGN_TOP_LEFT, 8, 6);
    s_wifi_ta = lv_textarea_create(s_pass_box);
    lv_obj_set_size(s_wifi_ta, 200, 32);
    lv_obj_align(s_wifi_ta, LV_ALIGN_TOP_MID, 0, 28);
    lv_textarea_set_one_line(s_wifi_ta, true);
    lv_textarea_set_max_length(s_wifi_ta, 16);
    lv_textarea_set_placeholder_text(s_wifi_ta, "digits");
    lv_obj_add_event_cb(s_wifi_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *okb = lv_btn_create(s_pass_box);
    lv_obj_set_size(okb, 96, 30);
    lv_obj_align(okb, LV_ALIGN_BOTTOM_LEFT, 8, -6);
    lv_obj_add_event_cb(okb, join_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *okl = lv_label_create(okb); lv_label_set_text(okl, "CONNECT"); lv_obj_center(okl);
    lv_obj_t *clb = lv_btn_create(s_pass_box);
    lv_obj_set_size(clb, 96, 30);
    lv_obj_align(clb, LV_ALIGN_BOTTOM_RIGHT, -8, -6);
    lv_obj_add_event_cb(clb, pass_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cll = lv_label_create(clb); lv_label_set_text(cll, "CLOSE"); lv_obj_center(cll);
    lv_obj_add_flag(s_pass_box, LV_OBJ_FLAG_HIDDEN);
    s_ip_box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_ip_box, 200, 36);
    lv_obj_align(s_ip_box, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_add_flag(s_ip_box, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_ip_box, pass_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_style_bg_color(s_ip_box, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(s_ip_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_ip_box, lv_color_hex(0x1F6FEB), 0);
    lv_obj_set_style_border_width(s_ip_box, 2, 0);
    s_ip_msg = lv_label_create(s_ip_box);
    lv_label_set_text(s_ip_msg, "IP");
    lv_label_set_long_mode(s_ip_msg, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_ip_msg, 200);
    lv_obj_align(s_ip_msg, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_ip_box, LV_OBJ_FLAG_HIDDEN);
    make_pad(lv_layer_top());
    s_ota_box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_ota_box, 220, 86);
    lv_obj_align(s_ota_box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_ota_box, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(s_ota_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_ota_box, 12, 0);
    lv_obj_set_style_border_width(s_ota_box, 2, 0);
    lv_obj_set_style_border_color(s_ota_box, lv_color_hex(0x1F6F5B), 0);
    lv_obj_clear_flag(s_ota_box, LV_OBJ_FLAG_SCROLLABLE);
    s_ota_txt = lv_label_create(s_ota_box);
    lv_label_set_text(s_ota_txt, "OTA 0%");
    lv_obj_set_width(s_ota_txt, 200);
    lv_obj_set_style_text_align(s_ota_txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_ota_txt, LV_ALIGN_TOP_MID, 0, 10);
    s_ota_bar = lv_bar_create(s_ota_box);
    lv_obj_set_size(s_ota_bar, 190, 18);
    lv_obj_align(s_ota_bar, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_bar_set_range(s_ota_bar, 0, 100);
    lv_obj_set_style_bg_color(s_ota_bar, lv_color_hex(0x1F6F5B), LV_PART_INDICATOR);
    lv_obj_add_flag(s_ota_box, LV_OBJ_FLAG_HIDDEN);
}

void display_ota_progress(int percent, const char *msg)
{
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    safe_ota_set_percent(percent);
    if (!s_lvgl_lock || !display_lock()) return;
    if (s_ota_box) {
        lv_obj_clear_flag(s_ota_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_ota_box);
    }
    if (s_ota_bar) lv_bar_set_value(s_ota_bar, percent, LV_ANIM_ON);
    if (s_ota_txt) {
        char b[40];
        snprintf(b, sizeof(b), "%s %d%%", msg ? msg : "OTA", percent);
        lv_label_set_text(s_ota_txt, b);
    }
    display_unlock();
}

void display_ota_hide(void)
{
    safe_ota_set_percent(-1);
    if (!s_lvgl_lock || !display_lock()) return;
    if (s_ota_box) lv_obj_add_flag(s_ota_box, LV_OBJ_FLAG_HIDDEN);
    display_unlock();
}

esp_err_t display_init(void)
{
    s_lvgl_lock = xSemaphoreCreateRecursiveMutex();
    if (!s_lvgl_lock) return ESP_ERR_NO_MEM;

    spi_bus_config_t bus = {
        .mosi_io_num = CUBE_LCD_MOSI_GPIO,
        .miso_io_num = CUBE_LCD_MISO_GPIO,
        .sclk_io_num = CUBE_LCD_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CUBE_LCD_MAX_TRANSFER,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = CUBE_LCD_CS_GPIO,
        .dc_gpio_num = CUBE_LCD_DC_GPIO,
        .spi_mode = 0,
        .pclk_hz = CUBE_LCD_SPI_HZ,
        .trans_queue_depth = CUBE_LCD_QUEUE_DEPTH,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .on_color_trans_done = lcd_flush_done,
        .user_ctx = &s_disp_drv,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_panel_io));

#if CUBE_LCD_USE_ST7789
    ESP_LOGW(TAG, "panel create ST7789 240x280 gap_y...");
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CUBE_LCD_RST_GPIO,
        .rgb_endian = CUBE_LCD_BGR ? LCD_RGB_ENDIAN_BGR : LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_panel_io, &panel_cfg, &s_panel));
#else
    ESP_LOGW(TAG, "panel create ILI9341...");
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CUBE_LCD_RST_GPIO,
        .rgb_endian = CUBE_LCD_BGR ? LCD_RGB_ENDIAN_BGR : LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(s_panel_io, &panel_cfg, &s_panel));
#endif

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, CUBE_LCD_INVERT));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, CUBE_LCD_MIRROR_X, CUBE_LCD_MIRROR_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, CUBE_LCD_GAP_X, CUBE_LCD_GAP_Y));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
    ESP_LOGW(TAG, "panel on invert=%d bgr=%d mirror=%d,%d", CUBE_LCD_INVERT, CUBE_LCD_BGR,
             CUBE_LCD_MIRROR_X, CUBE_LCD_MIRROR_Y);

    bl_init();

    temperature_sensor_config_t tcfg = TEMPERATURE_SENSOR_CONFIG_DEFAULT(-10, 80);
    if (temperature_sensor_install(&tcfg, &s_temp) == ESP_OK) {
        temperature_sensor_enable(s_temp);
    }

    lv_init();
    size_t buf_px = CUBE_LCD_H_RES * 40;
    s_buf1 = heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_DMA);
    s_buf2 = heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (!s_buf1 || !s_buf2) return ESP_ERR_NO_MEM;
    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, buf_px);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = CUBE_LCD_H_RES;
    s_disp_drv.ver_res = CUBE_LCD_V_RES;
    s_disp_drv.flush_cb = lv_flush;
    s_disp_drv.draw_buf = &s_draw_buf;
    s_display = lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_touch_drv);
    s_touch_drv.type = LV_INDEV_TYPE_POINTER;
    s_touch_drv.read_cb = touch_lvgl_read;
    lv_indev_drv_register(&s_touch_drv);

    const esp_timer_create_args_t tick_args = {
        .callback = tick_cb, .name = "lv_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &s_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_tick_timer, CUBE_LVGL_TICK_MS * 1000));

    ESP_LOGW(TAG, "LVGL mem %u KiB, building UI", (unsigned)(LV_MEM_SIZE / 1024));
    build_ui();
    ESP_LOGI(TAG, "LCD portrait %dx%d (ST7789=%d)", CUBE_LCD_H_RES, CUBE_LCD_V_RES, CUBE_LCD_USE_ST7789);
    sm_start_session();
    return ESP_OK;
}

void display_set_values(void)
{
    if (!display_lock()) return;
    char b[64];
    float tc = read_temp_c();
    size_t heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int load = cpu_load_pct();
    int ch = web_get_ap_channel();
    cube_state_snapshot_t st;
    app_state_snapshot(&st);
    bool live = st.ccs_output_voltage_raw > 0;
    float uv = live ? st.ccs_output_voltage_raw / 10.0f : 0;
    float ia = st.ccs_output_current_0p1a / 10.0f;
    if (ia < 0) ia = -ia;
    if (s_out_v) lv_label_set_text(s_out_v, live ? (snprintf(b, sizeof(b), "выход %.0f В", (double)uv), b) : "выход -- В");
    if (s_out_a) lv_label_set_text(s_out_a, live ? (snprintf(b, sizeof(b), "выход %.1f А", (double)ia), b) : "выход -- А");
    if (s_req_v) { snprintf(b, sizeof(b), "запрос %d В", s_set_v); lv_label_set_text(s_req_v, b); }
    if (s_req_i) { snprintf(b, sizeof(b), "запрос %d А", s_set_i); lv_label_set_text(s_req_i, b); }
    if (s_time) { snprintf(b, sizeof(b), "время %u мин", (unsigned)st.ccs_charge_time_min); lv_label_set_text(s_time, b); }
    if (s_stat) { snprintf(b, sizeof(b), "статус %u %s", (unsigned)st.main_state, cube_main_state_name(st.main_state)); lv_label_set_text(s_stat, b); }
    if (s_voltage) lv_label_set_text(s_voltage, live ? (snprintf(b, sizeof(b), "%.0f", (double)uv), b) : "--");
    if (s_current) lv_label_set_text(s_current, live ? (snprintf(b, sizeof(b), "%.1f", (double)ia), b) : "--");
    if (s_power) {
        snprintf(b, sizeof(b), live ? "мощность %.0f Вт" : "мощность -- Вт", (double)(uv * ia));
        lv_label_set_text(s_power, b);
    }
    if (s_soc) {
        snprintf(b, sizeof(b), "SOC %u%%  E %.2f", (unsigned)st.soc_percent, (double)app_state_energy_kwh());
        lv_label_set_text(s_soc, b);
    }
    if (s_ses_kwh) {
        snprintf(b, sizeof(b), "кВтч %.3f", (double)app_state_energy_kwh());
        lv_label_set_text(s_ses_kwh, b);
    }
    if (s_ses_min) {
        snprintf(b, sizeof(b), "время %u мин", (unsigned)st.ccs_charge_time_min);
        lv_label_set_text(s_ses_min, b);
    }
    if (s_ses_soc) {
        snprintf(b, sizeof(b), "оценка %u%% / %.1f", (unsigned)st.soc_percent, (double)app_state_cap_kwh());
        lv_label_set_text(s_ses_soc, b);
    }
    if (s_log_lbl) {
        char logb[160];
        app_state_log_text(logb, (int)sizeof(logb));
        lv_label_set_text(s_log_lbl, logb);
    }
    if (s_can_lbl) {
        cube_can_frame_t fr[4];
        size_t n = 0;
        char cb[180];
        int o = 0;
        if (gbt_can_get_history(fr, 4, &n) != ESP_OK) n = 0;
        if (n == 0) {
            snprintf(cb, sizeof(cb), "нет кадров");
        } else {
            cb[0] = 0;
            for (size_t i = 0; i < n && o < (int)sizeof(cb) - 28; ++i) {
                o += snprintf(cb + o, sizeof(cb) - (size_t)o, "%08lX %u %02X%02X\n",
                              (unsigned long)fr[i].id, (unsigned)fr[i].dlc,
                              fr[i].dlc ? fr[i].data[0] : 0, fr[i].dlc > 1 ? fr[i].data[1] : 0);
            }
        }
        lv_label_set_text(s_can_lbl, cb);
    }
    if (s_stage) {
        snprintf(b, sizeof(b), "%s %s %s", cube_protocol_name(st.protocol), cube_main_state_name(st.main_state), (st.rx_count & 1) ? "<>" : "--");
        lv_label_set_text(s_stage, b);
    }
    if (s_relay) {
        snprintf(b, sizeof(b), (st.relay_a || st.relay_b) ? "выход ON" : "выход OFF");
        lv_label_set_text(s_relay, b);
    }
    if (s_cml) {
        snprintf(b, sizeof(b), "CML %.1f V / %.1f A",
                 st.cml_max_voltage_raw / 10.0, st.cml_max_current_0p1a / 10.0);
        lv_label_set_text(s_cml, b);
    }
    if (s_can) {
        snprintf(b, sizeof(b), "CAN rx%lu tx%lu", (unsigned long)st.rx_count, (unsigned long)st.tx_count);
        lv_label_set_text(s_can, b);
    }
    if (s_ip_lbl) {
        snprintf(b, sizeof(b), "STA IP %s", web_sta_ip());
        lv_label_set_text(s_ip_lbl, b);
    }

    if (s_sys_temp) {
        if (tc > -50) snprintf(b, sizeof(b), "CPU: %.0f C", (double)tc);
        else snprintf(b, sizeof(b), "CPU: -- C");
        lv_label_set_text(s_sys_temp, b);
    }
    if (s_sys_cpu) {
        snprintf(b, sizeof(b), "F: %d MHz", (int)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
        lv_label_set_text(s_sys_cpu, b);
    }
    if (s_sys_load) {
        snprintf(b, sizeof(b), "Load: ~%d %%", load);
        lv_label_set_text(s_sys_load, b);
    }
    if (s_sys_heap) {
        snprintf(b, sizeof(b), "Heap: %u KB", (unsigned)(heap / 1024));
        lv_label_set_text(s_sys_heap, b);
    }
    if (s_sys_wifi) {
        snprintf(b, sizeof(b), "AP: %s ch%d", CUBE_WIFI_SSID, ch > 0 ? ch : CUBE_WIFI_CHANNEL);
        lv_label_set_text(s_sys_wifi, b);
    }
    if (s_sys_touch) {
        lv_label_set_text(s_sys_touch, touch_is_available() ? "Touch: OK" : "Touch: --");
    }
    if (s_status) {
        snprintf(b, sizeof(b), "VIN %s", app_state_vin());
        lv_label_set_text(s_status, b);
    }
    if (s_ip_box && s_ip_msg && !s_ip_shown && web_sta_ip()[0] >= '1' && web_sta_ip()[0] <= '9') {
        char msg[64];
        snprintf(msg, sizeof(msg), "LAN %s", web_sta_ip());
        lv_label_set_text(s_ip_msg, msg);
        lv_obj_clear_flag(s_ip_box, LV_OBJ_FLAG_HIDDEN);
        s_ip_shown = 1;
        if (s_pass_box) lv_obj_add_flag(s_pass_box, LV_OBJ_FLAG_HIDDEN);
    }
    /* Brightness stays on the SYS slider; do not override it. */
    (void)tc;
    display_unlock();
}

void display_task(void *arg)
{
    (void)arg;
    uint32_t n = 0;
    for (;;) {
        if (display_lock()) {
            lv_timer_handler();
            display_unlock();
        }
        if (s_pending_boot) {
            vTaskDelay(pdMS_TO_TICKS(500));
            web_boot_original();
        }
        if ((n++ % 20) == 0) display_set_values();
        vTaskDelay(pdMS_TO_TICKS(CUBE_LVGL_TASK_MS));
    }
}
