#include "display.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "config.h"
#include "sdkconfig.h"
#include "web.h"
#include "esp_wifi.h"
#include "driver/temperature_sensor.h"
#include "app_state.h"
#include "settings.h"
#include "diag.h"
#include "driver/ledc.h"
#include "touch_cst816s.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lv_font_ru_14.h"

static const char *TAG = "DISPLAY";
static esp_lcd_panel_handle_t s_panel;
static esp_lcd_panel_io_handle_t s_panel_io;
static SemaphoreHandle_t s_lvgl_lock;
static lv_disp_t *s_display;
static lv_obj_t *s_voltage;
static lv_obj_t *s_current;
static lv_obj_t *s_power;
static lv_obj_t *s_stage;
static lv_obj_t *s_request;
static lv_obj_t *s_time;
static lv_obj_t *s_status;
static lv_obj_t *s_can;
static lv_obj_t *s_page_chg;
static lv_obj_t *s_page_sys;
static lv_obj_t *s_sys_temp;
static lv_obj_t *s_sys_freq;
static lv_obj_t *s_sys_wifi;
static lv_obj_t *s_sys_mac;
static lv_obj_t *s_sys_heap;
static lv_obj_t *s_sys_touch;
static lv_obj_t *s_sys_status;
static lv_obj_t *s_sys_brightness;
static lv_obj_t *s_page_wifi;
static lv_obj_t *s_wifi_list_lab[8];
static lv_obj_t *s_wifi_status;
static lv_obj_t *s_wifi_ip;
static lv_obj_t *s_wifi_ta;
static lv_obj_t *s_wifi_kb;
static lv_obj_t *s_wifi_kb_hide;
static int s_wifi_sel;
static int s_wifi_count;
static web_scan_ap_t s_wifi_aps[WEB_SCAN_MAX];
static int s_ui_page; /* 0=charge 1=sys 2=wifi */
static lv_color_t *s_buf1;
static lv_color_t *s_buf2;
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_touch_drv;
static esp_timer_handle_t s_tick_timer;

static bool lcd_flush_done(esp_lcd_panel_io_handle_t panel_io,
                           esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    (void)panel_io;
    (void)edata;
    /* Only signal LVGL after display driver is registered. Smoke-fill and
       early panel draws run with the same SPI callback; calling
       lv_disp_flush_ready on an unregistered drv causes Store access fault. */
    if (!s_display) {
        return false;
    }
    lv_disp_drv_t *drv = (lv_disp_drv_t *)user_ctx;
    if (drv) {
        lv_disp_flush_ready(drv);
    }
    return false;
}

static void lv_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
    (void)drv;
    /* Must NOT call lv_disp_flush_ready here — DMA still owns the buffer.
       lcd_flush_done() signals ready when SPI transfer completes. */
    esp_err_t err = esp_lcd_panel_draw_bitmap(s_panel, area->x1, area->y1,
                                               area->x2 + 1, area->y2 + 1, color_p);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "LCD flush failed: %s", esp_err_to_name(err));
        if (s_display) lv_disp_flush_ready(s_display->driver);
    }
}

static void tick_cb(void *arg) {
    (void)arg;
    lv_tick_inc(1);
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int size) {
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, text);
    lv_obj_set_style_text_font(o, &lv_font_ru_14, 0); (void)size;
    lv_obj_set_style_text_color(o, lv_color_hex(0xE7EEF7), 0);
    return o;
}

static lv_obj_t *make_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                           const char *caption, lv_obj_t **value_out) {
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x111F2C), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x21364B), 0);
    lv_obj_set_style_pad_all(card, 6, 0);

    lv_obj_t *cap = make_label(card, caption, 14);
    lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 0, 0);
    *value_out = make_label(card, "--", 28);
    lv_obj_align(*value_out, LV_ALIGN_BOTTOM_LEFT, 0, -2);
    return card;
}

static void page_show(int page)
{
    s_ui_page = page;
    if (s_page_chg) {
        if (page == 0) lv_obj_clear_flag(s_page_chg, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_page_chg, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_page_sys) {
        if (page == 1) lv_obj_clear_flag(s_page_sys, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_page_sys, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_page_wifi) {
        if (page == 2) lv_obj_clear_flag(s_page_wifi, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_page_wifi, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_wifi_kb) {
        if (page == 2) { /* keep hidden until password */ }
        else lv_obj_add_flag(s_wifi_kb, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_wifi_kb_hide && page != 2) lv_obj_add_flag(s_wifi_kb_hide, LV_OBJ_FLAG_HIDDEN);
}


static void on_btn_sys(lv_event_t *e)
{
    (void)e;
    page_show(1);
}

static void on_btn_chg(lv_event_t *e)
{
    (void)e;
    page_show(0);
}

static void on_btn_wifi_rst(lv_event_t *e)
{
    (void)e;
    if (s_sys_status) lv_label_set_text(s_sys_status, "WiFi: restart...");
    esp_err_t err = web_wifi_restart();
    if (s_sys_status) {
        if (err == ESP_OK) lv_label_set_text(s_sys_status, "WiFi: restarting...");
        else if (err == ESP_ERR_INVALID_STATE) lv_label_set_text(s_sys_status, "WiFi: busy");
        else lv_label_set_text(s_sys_status, "WiFi: restart FAIL");
    }
}


static void on_btn_wifi_page(lv_event_t *e)
{
    (void)e;
    page_show(2);
}

static void wifi_list_refresh_labels(void)
{
    for (int i = 0; i < 8; ++i) {
        if (!s_wifi_list_lab[i]) continue;
        if (i < s_wifi_count) {
            const char *mark = (i == s_wifi_sel) ? ">" : " ";
            lv_label_set_text_fmt(s_wifi_list_lab[i], "%s%s %ddBm ch%u",
                                  mark, s_wifi_aps[i].ssid,
                                  (int)s_wifi_aps[i].rssi,
                                  (unsigned)s_wifi_aps[i].channel);
        } else {
            lv_label_set_text(s_wifi_list_lab[i], "");
        }
    }
}

static void on_btn_scan(lv_event_t *e)
{
    (void)e;
    if (s_wifi_status) lv_label_set_text(s_wifi_status, "Scan...");
    s_wifi_count = web_wifi_scan(s_wifi_aps, WEB_SCAN_MAX);
    if (s_wifi_count > 6) s_wifi_count = 6;
    s_wifi_sel = 0;
    wifi_list_refresh_labels();
    if (s_wifi_status) {
        lv_label_set_text_fmt(s_wifi_status, "Found %d AP  SoftAP ch=%u",
                              s_wifi_count, (unsigned)web_wifi_ap_channel());
    }
}

static void on_btn_sel_up(lv_event_t *e)
{
    (void)e;
    if (s_wifi_count <= 0) return;
    if (s_wifi_sel > 0) s_wifi_sel--;
    wifi_list_refresh_labels();
}

static void on_btn_sel_dn(lv_event_t *e)
{
    (void)e;
    if (s_wifi_count <= 0) return;
    if (s_wifi_sel + 1 < s_wifi_count) s_wifi_sel++;
    wifi_list_refresh_labels();
}

static void on_btn_connect(lv_event_t *e)
{
    (void)e;
    if (s_wifi_count <= 0 || s_wifi_sel < 0 || s_wifi_sel >= s_wifi_count) {
        if (s_wifi_status) lv_label_set_text(s_wifi_status, "Select AP first");
        return;
    }
    const char *pass = "";
    if (s_wifi_ta) pass = lv_textarea_get_text(s_wifi_ta);
    if (s_wifi_status) {
        lv_label_set_text_fmt(s_wifi_status, "Connect %s...", s_wifi_aps[s_wifi_sel].ssid);
    }
    esp_err_t err = web_wifi_sta_connect(s_wifi_aps[s_wifi_sel].ssid, pass ? pass : "");
    if (err != ESP_OK && s_wifi_status) {
        lv_label_set_text_fmt(s_wifi_status, "Connect err %s", esp_err_to_name(err));
    }
}

static void on_ta_focused(lv_event_t *e)
{
    (void)e;
    if (s_wifi_kb) {
        lv_obj_clear_flag(s_wifi_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_wifi_kb);
    }
    if (s_wifi_kb_hide) {
        lv_obj_clear_flag(s_wifi_kb_hide, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(s_wifi_kb_hide);
    }
}

static void on_kb_hide(lv_event_t *e)
{
    (void)e;
    if (s_wifi_kb) lv_obj_add_flag(s_wifi_kb, LV_OBJ_FLAG_HIDDEN);
    if (s_wifi_kb_hide) lv_obj_add_flag(s_wifi_kb_hide, LV_OBJ_FLAG_HIDDEN);
    if (s_wifi_ta) lv_obj_clear_state(s_wifi_ta, LV_STATE_FOCUSED);
}

static void on_kb_cancel(lv_event_t *e)
{
    on_kb_hide(e);
}

static void on_brightness_changed(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t value = (uint8_t)lv_slider_get_value(slider);
    (void)display_set_brightness(value);
    if (s_sys_brightness) lv_label_set_text_fmt(s_sys_brightness, "Яркость: %u%%", (unsigned)value);
}

static void on_brightness_released(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t value = (uint8_t)lv_slider_get_value(slider);
    (void)settings_set_brightness(value);
}

static void ui_create(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* --- page 0: charge --- */
    s_page_chg = lv_obj_create(scr);
    lv_obj_set_size(s_page_chg, CUBE_LCD_H_RES, CUBE_LCD_V_RES - 36);
    lv_obj_set_pos(s_page_chg, 0, 0);
    lv_obj_set_style_bg_opa(s_page_chg, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_page_chg, 0, 0);
    lv_obj_set_style_pad_all(s_page_chg, 0, 0);
    lv_obj_clear_flag(s_page_chg, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = make_label(s_page_chg, "ГБ/Т Зарядка", 14);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    (void)make_card(s_page_chg, 8, 24, 112, 64, "НАПРЯЖЕНИЕ", &s_voltage);
    (void)make_card(s_page_chg, 120, 24, 112, 64, "ТОК", &s_current);

    s_power = make_label(s_page_chg, "МОЩН. ---- Вт", 14);
    lv_obj_align(s_power, LV_ALIGN_TOP_LEFT, 10, 96);
    s_stage = make_label(s_page_chg, "ЭТАП --", 14);
    lv_obj_align(s_stage, LV_ALIGN_TOP_LEFT, 10, 118);
    s_request = make_label(s_page_chg, "ЗАПРОС --.-В --.-А", 14);
    lv_obj_align(s_request, LV_ALIGN_TOP_LEFT, 10, 140);
    s_time = make_label(s_page_chg, "ВРЕМЯ ---- мин", 14);
    lv_obj_align(s_time, LV_ALIGN_TOP_LEFT, 10, 162);
    s_status = make_label(s_page_chg, "СТАТУС --", 14);
    lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 10, 184);
    s_can = make_label(s_page_chg, "CAN 250к RX 0", 14);
    lv_obj_align(s_can, LV_ALIGN_TOP_LEFT, 10, 206);

    char wifi_line[48];
    if (strlen(CUBE_WIFI_PASSWORD) >= 8U) {
        (void)snprintf(wifi_line, sizeof(wifi_line), "WiFi %s / %s", CUBE_WIFI_SSID, CUBE_WIFI_PASSWORD);
    } else {
        (void)snprintf(wifi_line, sizeof(wifi_line), "WiFi %s OPEN", CUBE_WIFI_SSID);
    }
    lv_obj_t *hint = make_label(s_page_chg, wifi_line, 14);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 10, 228);

    /* --- page 1: system / RF diag --- */
    s_page_sys = lv_obj_create(scr);
    lv_obj_set_size(s_page_sys, CUBE_LCD_H_RES, CUBE_LCD_V_RES - 36);
    lv_obj_set_pos(s_page_sys, 0, 0);
    lv_obj_set_style_bg_opa(s_page_sys, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_page_sys, 0, 0);
    lv_obj_set_style_pad_all(s_page_sys, 0, 0);
    lv_obj_clear_flag(s_page_sys, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_page_sys, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *st = make_label(s_page_sys, "СИСТЕМА / WiFi", 14);
    lv_obj_align(st, LV_ALIGN_TOP_MID, 0, 4);

    s_sys_temp = make_label(s_page_sys, "Темп. CPU: -- C", 14);
    lv_obj_align(s_sys_temp, LV_ALIGN_TOP_LEFT, 10, 32);
    s_sys_freq = make_label(s_page_sys, "CPU: -- MHz", 14);
    lv_obj_align(s_sys_freq, LV_ALIGN_TOP_LEFT, 10, 54);
    s_sys_wifi = make_label(s_page_sys, "WiFi ch=--", 14);
    lv_obj_align(s_sys_wifi, LV_ALIGN_TOP_LEFT, 10, 76);
    s_sys_mac = make_label(s_page_sys, "MAC --", 14);
    lv_obj_align(s_sys_mac, LV_ALIGN_TOP_LEFT, 10, 98);
    s_sys_heap = make_label(s_page_sys, "Heap --", 14);
    lv_obj_align(s_sys_heap, LV_ALIGN_TOP_LEFT, 10, 120);
    s_sys_touch = make_label(s_page_sys, "Touch: --", 14);
    lv_obj_align(s_sys_touch, LV_ALIGN_TOP_LEFT, 10, 142);
    s_sys_status = make_label(s_page_sys, "WiFi: idle", 14);
    lv_obj_align(s_sys_status, LV_ALIGN_TOP_LEFT, 10, 164);

    s_sys_brightness = make_label(s_page_sys, "Яркость: 100%", 14);
    lv_obj_align(s_sys_brightness, LV_ALIGN_TOP_LEFT, 10, 186);
    lv_obj_t *brightness = lv_slider_create(s_page_sys);
    lv_obj_set_width(brightness, 200);
    lv_obj_set_height(brightness, 18);
    lv_obj_align(brightness, LV_ALIGN_TOP_MID, 0, 208);
    lv_slider_set_range(brightness, 10, 100);
    lv_slider_set_value(brightness, settings_get_brightness(), LV_ANIM_OFF);
    lv_label_set_text_fmt(s_sys_brightness, "Яркость: %u%%", (unsigned)settings_get_brightness());
    lv_obj_add_event_cb(brightness, on_brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(brightness, on_brightness_released, LV_EVENT_RELEASED, NULL);

    lv_obj_t *btn_rst = lv_btn_create(s_page_sys);
    lv_obj_set_size(btn_rst, 200, 40);
    lv_obj_align(btn_rst, LV_ALIGN_TOP_MID, 0, 238);
    lv_obj_add_event_cb(btn_rst, on_btn_wifi_rst, LV_EVENT_CLICKED, NULL);
    lv_obj_t *bl = lv_label_create(btn_rst);
    lv_label_set_text(bl, "RESTART WiFi");
    lv_obj_set_style_text_font(bl, &lv_font_ru_14, 0);
    lv_obj_center(bl);


    /* --- page 2: WiFi debug / STA --- */
    s_page_wifi = lv_obj_create(scr);
    lv_obj_set_size(s_page_wifi, CUBE_LCD_H_RES, CUBE_LCD_V_RES - 36);
    lv_obj_set_pos(s_page_wifi, 0, 0);
    lv_obj_set_style_bg_opa(s_page_wifi, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_page_wifi, 0, 0);
    lv_obj_set_style_pad_all(s_page_wifi, 0, 0);
    lv_obj_clear_flag(s_page_wifi, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_page_wifi, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *wt = make_label(s_page_wifi, "WiFi SCAN / STA", 14);
    lv_obj_align(wt, LV_ALIGN_TOP_MID, 0, 2);

    for (int i = 0; i < 6; ++i) {
        s_wifi_list_lab[i] = make_label(s_page_wifi, "", 14);
        lv_obj_align(s_wifi_list_lab[i], LV_ALIGN_TOP_LEFT, 4, 20 + i * 16);
    }

    s_wifi_status = make_label(s_page_wifi, "Scan for APs", 14);
    lv_obj_align(s_wifi_status, LV_ALIGN_TOP_LEFT, 4, 150);
    s_wifi_ip = make_label(s_page_wifi, "STA IP: -", 14);
    lv_obj_align(s_wifi_ip, LV_ALIGN_TOP_LEFT, 4, 136);

    s_wifi_ta = lv_textarea_create(s_page_wifi);
    lv_obj_set_size(s_wifi_ta, 230, 28);
    lv_obj_align(s_wifi_ta, LV_ALIGN_TOP_LEFT, 4, 186);
    lv_textarea_set_one_line(s_wifi_ta, true);
    lv_textarea_set_password_mode(s_wifi_ta, false);
    lv_textarea_set_placeholder_text(s_wifi_ta, "password");
    lv_obj_add_event_cb(s_wifi_ta, on_ta_focused, LV_EVENT_FOCUSED, NULL);

    lv_obj_t *bscan = lv_btn_create(s_page_wifi);
    lv_obj_set_size(bscan, 70, 26);
    lv_obj_align(bscan, LV_ALIGN_TOP_LEFT, 4, 194);
    lv_obj_add_event_cb(bscan, on_btn_scan, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l1 = lv_label_create(bscan); lv_label_set_text(l1, "SCAN"); lv_obj_center(l1);

    lv_obj_t *bup = lv_btn_create(s_page_wifi);
    lv_obj_set_size(bup, 40, 26);
    lv_obj_align(bup, LV_ALIGN_TOP_LEFT, 80, 194);
    lv_obj_add_event_cb(bup, on_btn_sel_up, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l2 = lv_label_create(bup); lv_label_set_text(l2, "UP"); lv_obj_center(l2);

    lv_obj_t *bdn = lv_btn_create(s_page_wifi);
    lv_obj_set_size(bdn, 40, 26);
    lv_obj_align(bdn, LV_ALIGN_TOP_LEFT, 126, 194);
    lv_obj_add_event_cb(bdn, on_btn_sel_dn, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l3 = lv_label_create(bdn); lv_label_set_text(l3, "DN"); lv_obj_center(l3);

    lv_obj_t *bcon = lv_btn_create(s_page_wifi);
    lv_obj_set_size(bcon, 60, 26);
    lv_obj_align(bcon, LV_ALIGN_TOP_LEFT, 172, 194);
    lv_obj_add_event_cb(bcon, on_btn_connect, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l4 = lv_label_create(bcon); lv_label_set_text(l4, "JOIN"); lv_obj_center(l4);

    s_wifi_kb = lv_keyboard_create(scr);
    lv_obj_set_size(s_wifi_kb, CUBE_LCD_H_RES, 150);
    lv_obj_align(s_wifi_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(s_wifi_kb, s_wifi_ta);
    lv_keyboard_set_mode(s_wifi_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_add_event_cb(s_wifi_kb, on_kb_cancel, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(s_wifi_kb, on_kb_cancel, LV_EVENT_READY, NULL);
    lv_obj_add_flag(s_wifi_kb, LV_OBJ_FLAG_HIDDEN);

    s_wifi_kb_hide = lv_btn_create(scr);
    lv_obj_set_size(s_wifi_kb_hide, CUBE_LCD_H_RES, 24);
    lv_obj_align(s_wifi_kb_hide, LV_ALIGN_BOTTOM_MID, 0, -150);
    lv_obj_add_event_cb(s_wifi_kb_hide, on_kb_hide, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(s_wifi_kb_hide, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *kb_hide_label = lv_label_create(s_wifi_kb_hide);
    lv_label_set_text(kb_hide_label, "СКРЫТЬ КЛАВИАТУРУ");
    lv_obj_set_style_text_font(kb_hide_label, &lv_font_ru_14, 0);
    lv_obj_center(kb_hide_label);

    /* --- bottom nav --- */
    lv_obj_t *btn_ch = lv_btn_create(scr);
    lv_obj_set_size(btn_ch, 72, 28);
    lv_obj_align(btn_ch, LV_ALIGN_BOTTOM_LEFT, 4, -3);
    lv_obj_add_event_cb(btn_ch, on_btn_chg, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lc = lv_label_create(btn_ch);
    lv_label_set_text(lc, "CHG");
    lv_obj_set_style_text_font(lc, &lv_font_ru_14, 0);
    lv_obj_center(lc);

    lv_obj_t *btn_sys = lv_btn_create(scr);
    lv_obj_set_size(btn_sys, 72, 28);
    lv_obj_align(btn_sys, LV_ALIGN_BOTTOM_MID, 0, -3);
    lv_obj_add_event_cb(btn_sys, on_btn_sys, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ls = lv_label_create(btn_sys);
    lv_label_set_text(ls, "SYS");
    lv_obj_set_style_text_font(ls, &lv_font_ru_14, 0);
    lv_obj_center(ls);

    lv_obj_t *btn_wf = lv_btn_create(scr);
    lv_obj_set_size(btn_wf, 72, 28);
    lv_obj_align(btn_wf, LV_ALIGN_BOTTOM_RIGHT, -4, -3);
    lv_obj_add_event_cb(btn_wf, on_btn_wifi_page, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lw = lv_label_create(btn_wf);
    lv_label_set_text(lw, "WiFi");
    lv_obj_set_style_text_font(lw, &lv_font_ru_14, 0);
    lv_obj_center(lw);

    s_ui_page = 0;
    s_wifi_sel = 0;
    s_wifi_count = 0;
    page_show(0);
}


static uint8_t s_brightness = 100;
static bool s_bl_pwm_ready;

static esp_err_t backlight_pwm_init(void)
{
#if CUBE_LCD_BL_PWM_GPIO >= 0
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .freq_hz = CUBE_LCD_BL_PWM_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer);
    if (err != ESP_OK) return err;
    ledc_channel_config_t ch = {
        .gpio_num = CUBE_LCD_BL_PWM_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 1023,
        .hpoint = 0,
    };
    err = ledc_channel_config(&ch);
    if (err == ESP_OK) {
        s_bl_pwm_ready = true;
        ESP_LOGW(TAG, "backlight PWM ready GPIO%d %dHz", CUBE_LCD_BL_PWM_GPIO, CUBE_LCD_BL_PWM_HZ);
    }
    return err;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

uint8_t display_get_brightness(void)
{
    return s_brightness;
}

esp_err_t display_set_brightness(uint8_t percent)
{
    if (percent < 10U) percent = 10U;
    if (percent > 100U) percent = 100U;
    s_brightness = percent;
#if CUBE_LCD_BL_PWM_GPIO >= 0
    if (s_bl_pwm_ready) {
        uint32_t duty = ((uint32_t)percent * 1023U) / 100U;
        esp_err_t err = ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
        if (err != ESP_OK) return err;
        return ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    }
#endif
    if (CUBE_LCD_BL_GPIO >= 0) {
        return gpio_set_level(CUBE_LCD_BL_GPIO, percent >= 50U ? 1 : 0);
    }
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t display_init(void) {
    if (s_panel) return ESP_OK;
    s_lvgl_lock = xSemaphoreCreateMutex();
    if (!s_lvgl_lock) return ESP_ERR_NO_MEM;

    /* Initialize LVGL driver metadata before panel IO creation.  The original
       firmware passes this exact driver object as SPI color-transfer callback context. */
    lv_init();
    lv_disp_drv_init(&s_disp_drv);

    const size_t draw_buf_pixels = (size_t)CUBE_LCD_H_RES * 20U;
    const size_t draw_buf_bytes = draw_buf_pixels * sizeof(lv_color_t);
    s_buf1 = (lv_color_t *)heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_DMA);
    s_buf2 = (lv_color_t *)heap_caps_malloc(draw_buf_bytes, MALLOC_CAP_DMA);
    if (!s_buf1 || !s_buf2) {
        if (s_buf1) { heap_caps_free(s_buf1); s_buf1 = NULL; }
        if (s_buf2) { heap_caps_free(s_buf2); s_buf2 = NULL; }
        return ESP_ERR_NO_MEM;
    }

    spi_bus_config_t buscfg = {
        .mosi_io_num = CUBE_LCD_MOSI_GPIO,
        .miso_io_num = CUBE_LCD_MISO_GPIO,
        .sclk_io_num = CUBE_LCD_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CUBE_LCD_MAX_TRANSFER,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num = CUBE_LCD_CS_GPIO,
        .dc_gpio_num = CUBE_LCD_DC_GPIO,
        .spi_mode = 0,
        .pclk_hz = CUBE_LCD_SPI_HZ,
        .trans_queue_depth = CUBE_LCD_QUEUE_DEPTH,
        .on_color_trans_done = lcd_flush_done,
        .user_ctx = &s_disp_drv,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &s_panel_io);
    if (err != ESP_OK) return err;

    /* Working path from release 2.0.5 (user saw pixels) + dump ILI9341:
       BGR + invert + mirror(Y) + BL GPIO2 AFTER panel init (strap already done). */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CUBE_LCD_RST_GPIO,
        .bits_per_pixel = 16,
#if CUBE_LCD_BGR
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
#else
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
#endif
    };
#if CUBE_LCD_USE_ST7789
    ESP_LOGW(TAG, "panel create ST7789 240x280 gap_y...");
    err = esp_lcd_new_panel_st7789(s_panel_io, &panel_cfg, &s_panel);
#else
    ESP_LOGW(TAG, "panel create ILI9341...");
    err = esp_lcd_new_panel_ili9341(s_panel_io, &panel_cfg, &s_panel);
#endif
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "panel create failed: %s", esp_err_to_name(err));
        return err;
    }
    if ((err = esp_lcd_panel_reset(s_panel)) != ESP_OK) return err;
    if ((err = esp_lcd_panel_init(s_panel)) != ESP_OK) return err;
    (void)esp_lcd_panel_set_gap(s_panel, CUBE_LCD_GAP_X, CUBE_LCD_GAP_Y);
    (void)esp_lcd_panel_invert_color(s_panel, CUBE_LCD_INVERT ? true : false);
    if ((err = esp_lcd_panel_mirror(s_panel,
                                    CUBE_LCD_MIRROR_X ? true : false,
                                    CUBE_LCD_MIRROR_Y ? true : false)) != ESP_OK) return err;
    if ((err = esp_lcd_panel_swap_xy(s_panel, false)) != ESP_OK) return err;
    if ((err = esp_lcd_panel_disp_on_off(s_panel, true)) != ESP_OK) return err;
    ESP_LOGW(TAG, "panel on invert=%d bgr=%d mirror=%d,%d",
             CUBE_LCD_INVERT, CUBE_LCD_BGR, CUBE_LCD_MIRROR_X, CUBE_LCD_MIRROR_Y);

    /* Backlight AFTER panel init — GPIO2 is strapping only at reset sample time. */
    if (CUBE_LCD_BL_GPIO >= 0) {
        gpio_config_t bl = {
            .pin_bit_mask = 1ULL << CUBE_LCD_BL_GPIO,
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        if (gpio_config(&bl) == ESP_OK) {
            (void)gpio_set_level(CUBE_LCD_BL_GPIO, 1);
            ESP_LOGW(TAG, "backlight GPIO%d = ON (post-init, post-strap)", CUBE_LCD_BL_GPIO);
        } else {
            ESP_LOGW(TAG, "backlight GPIO%d config failed", CUBE_LCD_BL_GPIO);
        }
    } else {
        ESP_LOGI(TAG, "backlight GPIO disabled");
    }
    if (CUBE_LCD_BL_PWM_GPIO >= 0) {
        if (backlight_pwm_init() == ESP_OK) {
            (void)display_set_brightness(settings_get_brightness());
        } else {
            ESP_LOGW(TAG, "backlight PWM unavailable; using digital backlight");
        }
    }

    /* Smoke test: several patterns. One of them should be visible if SPI wiring is OK. */
    {
        enum { LINE = 240 };
        uint16_t *line = heap_caps_malloc(LINE * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (line) {
            /* 1) Full white */
            for (int i = 0; i < LINE; ++i) line[i] = 0xFFFF;
            for (int y = 0; y < CUBE_LCD_V_RES; ++y)
                (void)esp_lcd_panel_draw_bitmap(s_panel, 0, y, CUBE_LCD_H_RES, y + 1, line);
            vTaskDelay(pdMS_TO_TICKS(80));
            /* 2) RGB bars (native 565) */
            static const uint16_t bars[3] = { 0xF800, 0x07E0, 0x001F };
            for (int b = 0; b < 3; ++b) {
                for (int i = 0; i < LINE; ++i) line[i] = bars[b];
                int y0 = b * 40;
                for (int y = y0; y < y0 + 40 && y < CUBE_LCD_V_RES; ++y)
                    (void)esp_lcd_panel_draw_bitmap(s_panel, 0, y, CUBE_LCD_H_RES, y + 1, line);
            }
            vTaskDelay(pdMS_TO_TICKS(80));
            /* 3) Byte-swapped bars (if COLOR_16_SWAP path differs from panel) */
            static const uint16_t bars_sw[3] = { 0x00F8, 0xE007, 0x1F00 };
            for (int b = 0; b < 3; ++b) {
                for (int i = 0; i < LINE; ++i) line[i] = bars_sw[b];
                int y0 = 120 + b * 40;
                for (int y = y0; y < y0 + 40 && y < CUBE_LCD_V_RES; ++y)
                    (void)esp_lcd_panel_draw_bitmap(s_panel, 0, y, CUBE_LCD_H_RES, y + 1, line);
            }
            heap_caps_free(line);
            ESP_LOGW(TAG, "panel smoke: white + RGB bars + swapped bars");
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }


    lv_disp_draw_buf_init(&s_draw_buf, s_buf1, s_buf2, draw_buf_pixels);
    s_disp_drv.hor_res = CUBE_LCD_H_RES;
    s_disp_drv.ver_res = CUBE_LCD_V_RES;
    s_disp_drv.flush_cb = lv_flush;
    s_disp_drv.draw_buf = &s_draw_buf;
    s_display = lv_disp_drv_register(&s_disp_drv);
    if (!s_display) return ESP_FAIL;

    lv_indev_drv_init(&s_touch_drv);
    s_touch_drv.type = LV_INDEV_TYPE_POINTER;
    s_touch_drv.read_cb = touch_lvgl_read;
    s_touch_drv.read_period = 10;
    if (!lv_indev_drv_register(&s_touch_drv)) return ESP_FAIL;

    (void)diag_init();
    ui_create();

    const esp_timer_create_args_t tick_args = {
        .callback = &tick_cb,
        .name = "lvgl_tick",
    };
    if ((err = esp_timer_create(&tick_args, &s_tick_timer)) != ESP_OK) return err;
    if ((err = esp_timer_start_periodic(s_tick_timer, 1000)) != ESP_OK) return err;

    app_state_set_display(true);
    ESP_LOGI(TAG, "LCD portrait %dx%d (ST7789=%d)", CUBE_LCD_H_RES, CUBE_LCD_V_RES, CUBE_LCD_USE_ST7789);
    return ESP_OK;
}

bool display_lock(void) {
    return s_lvgl_lock && xSemaphoreTake(s_lvgl_lock, pdMS_TO_TICKS(50)) == pdTRUE;
}

void display_unlock(void) {
    if (s_lvgl_lock) xSemaphoreGive(s_lvgl_lock);
}

void display_set_values(void) {
    if (!display_lock()) return;
    cube_state_snapshot_t s;
    app_state_snapshot(&s);

    if (s.ccs_output_voltage_raw) {
        lv_label_set_text_fmt(s_voltage, "%u.%u V",
                              (unsigned)(s.ccs_output_voltage_raw / 10U),
                              (unsigned)(s.ccs_output_voltage_raw % 10U));
    } else {
        lv_label_set_text(s_voltage, "--.- V");
    }

    if (s.ccs_output_current_0p1a == INT16_MIN) {
        lv_label_set_text(s_current, "--.- A");
    } else if (s.ccs_output_current_0p1a >= 0) {
        int value = s.ccs_output_current_0p1a;
        lv_label_set_text_fmt(s_current, "%d.%d A", value / 10, value % 10);
    } else {
        int value = -(int)s.ccs_output_current_0p1a;
        lv_label_set_text_fmt(s_current, "-%d.%d A", value / 10, value % 10);
    }

    long power_w = 0;
    if (s.ccs_output_voltage_raw && s.ccs_output_current_0p1a > 0) {
        power_w = ((long)s.ccs_output_voltage_raw * (long)s.ccs_output_current_0p1a) / 100L;
    }
    if (power_w > 0) lv_label_set_text_fmt(s_power, "POWER  %ld W", power_w);
    else lv_label_set_text(s_power, "POWER  ---- W");

    lv_label_set_text_fmt(s_stage, "ЭТАП: %s", cube_charge_stage_name(s.charger_stage));
    if (s.bcl_requested_voltage_raw || s.bcl_requested_current_0p1a != INT16_MIN) {
        int req_i = s.bcl_requested_current_0p1a;
        if (req_i == INT16_MIN) {
            lv_label_set_text_fmt(s_request, "REQ %u.%uV --.-A",
                                  (unsigned)(s.bcl_requested_voltage_raw / 10U),
                                  (unsigned)(s.bcl_requested_voltage_raw % 10U));
        } else if (req_i < 0) {
            int mag = -req_i;
            lv_label_set_text_fmt(s_request, "REQ %u.%uV -%d.%dA",
                                  (unsigned)(s.bcl_requested_voltage_raw / 10U),
                                  (unsigned)(s.bcl_requested_voltage_raw % 10U),
                                  mag / 10, mag % 10);
        } else {
            lv_label_set_text_fmt(s_request, "REQ %u.%uV %d.%dA",
                                  (unsigned)(s.bcl_requested_voltage_raw / 10U),
                                  (unsigned)(s.bcl_requested_voltage_raw % 10U),
                                  req_i / 10, req_i % 10);
        }
    } else {
        lv_label_set_text(s_request, "REQ  --.- V  --.- A");
    }
    if (s.last_ccs_ms) lv_label_set_text_fmt(s_time, "TIME  %u min", (unsigned)s.ccs_charge_time_min);
    else lv_label_set_text(s_time, "TIME  ---- min");
    lv_label_set_text_fmt(s_status, "STATUS  0x%02X  PERMIT %u",
                          (unsigned)s.ccs_status_raw, (unsigned)s.ccs_charge_permit);
    lv_label_set_text_fmt(s_can, "CAN 250k  RX %lu", (unsigned long)s.rx_count);


    /* System page live stats */
    if (s_ui_page == 1) {
        float tc = 0.0f;
        if (diag_get_temperature(&tc)) {
            int t10 = (int)(tc * 10.0f);
            if (t10 < 0) t10 = -t10;
            lv_label_set_text_fmt(s_sys_temp, "Temp CPU: %d.%d C",
                                  (int)tc, t10 % 10);
        } else {
            lv_label_set_text(s_sys_temp, "Temp CPU: n/a");
        }
        lv_label_set_text_fmt(s_sys_freq, "CPU: %u MHz  Load %u%%",
                              (unsigned)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ,
                              (unsigned)diag_get_cpu_load_percent());
        {
            uint8_t ch = CUBE_WIFI_CHANNEL;
            wifi_config_t wcfg = {0};
            if (esp_wifi_get_config(WIFI_IF_AP, &wcfg) == ESP_OK) {
                ch = wcfg.ap.channel;
            }
            lv_label_set_text_fmt(s_sys_wifi, "AP %s ch=%u %s",
                                  CUBE_WIFI_SSID, (unsigned)web_wifi_ap_channel(),
                                  (strlen(CUBE_WIFI_PASSWORD) >= 8U) ? "WPA2" : "OPEN");
        }
        {
            uint8_t mac[6] = {0};
            (void)esp_wifi_get_mac(WIFI_IF_AP, mac);
            lv_label_set_text_fmt(s_sys_mac, "MAC %02X:%02X:%02X:%02X:%02X:%02X",
                                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        }
        lv_label_set_text_fmt(s_sys_heap, "Heap free %u / min %u",
                              (unsigned)esp_get_free_heap_size(),
                              (unsigned)esp_get_minimum_free_heap_size());
        lv_label_set_text_fmt(s_sys_touch, "Touch: %s",
                              touch_is_available() ? "OK CST816" : "none");
    }

    if (s_ui_page == 2 && s_wifi_ip) {
        char ip[16];
        web_wifi_sta_ip(ip, sizeof(ip));
        if (ip[0]) {
            lv_label_set_text_fmt(s_wifi_ip, "STA IP: %s", ip);
            if (s_wifi_status && web_wifi_sta_connected()) {
                lv_label_set_text_fmt(s_wifi_status, "OK open http://%s/", ip);
            }
        } else {
            lv_label_set_text(s_wifi_ip, "STA IP: -");
        }
    }


    lv_timer_handler();
    display_unlock();

}

void display_task(void *arg) {
    (void)arg;
    for (;;) {
        display_set_values();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
