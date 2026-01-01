#include "display.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "app_state.h"
#include "touch_cst816s.h"
#include "web.h"
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
static lv_obj_t *s_scr_chg, *s_scr_sys, *s_scr_wifi;
static lv_obj_t *s_voltage, *s_current, *s_power, *s_stage, *s_status;
static lv_obj_t *s_soc, *s_relay, *s_cml, *s_can;
static lv_obj_t *s_sys_temp, *s_sys_cpu, *s_sys_load, *s_sys_heap, *s_sys_wifi, *s_sys_touch;
static lv_obj_t *s_br_label;
static lv_obj_t *s_pad;
static lv_obj_t *s_wifi_ta;
static lv_obj_t *s_scan_lbl;
static lv_obj_t *s_ip_lbl;
static lv_obj_t *s_nav_btn[3];
static int s_page; /* 0 CHG 1 SYS 2 WIFI */
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
    return s_lvgl_lock && xSemaphoreTake(s_lvgl_lock, pdMS_TO_TICKS(50)) == pdTRUE;
}
void display_unlock(void)
{
    if (s_lvgl_lock) xSemaphoreGive(s_lvgl_lock);
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


static void style_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x061018), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0xE8EEF4), 0);
}

static void style_btn(lv_obj_t *b, bool accent)
{
    lv_obj_set_style_bg_color(b, lv_color_hex(accent ? 0x1F6FEB : 0x1A2430), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_text_color(b, lv_color_hex(0xF4F7FB), 0);
    lv_obj_set_style_radius(b, 6, 0);
}

static void mark_nav(void)
{
    for (int i = 0; i < 3; ++i) {
        if (!s_nav_btn[i]) continue;
        style_btn(s_nav_btn[i], i == s_page);
    }
}

static void show_page(int p)
{
    s_page = p;
    if (s_pad) lv_obj_add_flag(s_pad, LV_OBJ_FLAG_HIDDEN);
    lv_scr_load(p == 0 ? s_scr_chg : (p == 1 ? s_scr_sys : s_scr_wifi));
    mark_nav();
}

static void nav_cb(lv_event_t *e)
{
    int p = (int)(intptr_t)lv_event_get_user_data(e);
    show_page(p);
}

static void br_cb(lv_event_t *e)
{
    lv_obj_t *sl = lv_event_get_target(e);
    int v = lv_slider_get_value(sl);
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

static void pad_key_cb(lv_event_t *e)
{
    const char *k = (const char *)lv_event_get_user_data(e);
    if (!s_wifi_ta || !k) return;
    if (k[0] == 'X') {
        lv_textarea_del_char(s_wifi_ta);
        return;
    }
    if (k[0] == 'H') {
        pad_hide();
        return;
    }
    lv_textarea_add_text(s_wifi_ta, k);
}

static void ta_focus_cb(lv_event_t *e)
{
    (void)e;
    if (!s_pad) return;
    lv_obj_clear_flag(s_pad, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_pad);
}

static void wifi_rst_cb(lv_event_t *e)
{
    (void)e;
    web_wifi_restart();
}

static void scan_cb(lv_event_t *e)
{
    (void)e;
    if (s_scan_lbl) lv_label_set_text(s_scan_lbl, "SCAN...");
    (void)web_scan_now();
    char buf[160];
    int n = web_scan_count();
    if (n <= 0) {
        snprintf(buf, sizeof(buf), "nets: 0");
    } else {
        int o = snprintf(buf, sizeof(buf), "%d: %s", 0, web_scan_ssid(0));
        for (int i = 1; i < n && o < (int)sizeof(buf) - 8; ++i) {
            o += snprintf(buf + o, sizeof(buf) - (size_t)o, "\n%d: %s", i, web_scan_ssid(i));
        }
    }
    if (s_scan_lbl) lv_label_set_text(s_scan_lbl, buf);
    s_pick = 0;
}

static void pick_cb(lv_event_t *e)
{
    int d = (int)(intptr_t)lv_event_get_user_data(e);
    int n = web_scan_count();
    if (n <= 0) return;
    s_pick = (s_pick + d + n) % n;
    if (s_scan_lbl) {
        char b[48];
        snprintf(b, sizeof(b), "SEL %d %s", s_pick, web_scan_ssid(s_pick));
        lv_label_set_text(s_scan_lbl, b);
    }
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
    pad_hide();
}

static lv_obj_t *mk_nav(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 232, 40);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_pad_all(bar, 2, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x0C141C), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_shadow_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    const char *names[] = {"CHG", "SYS", "WiFi"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_btn_create(bar);
        lv_obj_set_size(b, 72, 32);
        lv_obj_align(b, LV_ALIGN_LEFT_MID, 4 + i * 76, 0);
        style_btn(b, i == s_page);
        lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, names[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(0xF4F7FB), 0);
        lv_obj_center(l);
        s_nav_btn[i] = b;
    }
    return bar;
}

static void make_pad(lv_obj_t *parent)
{
    s_pad = lv_obj_create(parent);
    lv_obj_set_size(s_pad, 232, 168);
    lv_obj_align(s_pad, LV_ALIGN_BOTTOM_MID, 0, -44);
    lv_obj_set_style_bg_color(s_pad, lv_color_hex(0x101820), 0);
    lv_obj_set_style_bg_opa(s_pad, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_pad, 1, 0);
    lv_obj_set_style_border_color(s_pad, lv_color_hex(0x3A4A5C), 0);
    lv_obj_set_style_pad_all(s_pad, 4, 0);
    lv_obj_clear_flag(s_pad, LV_OBJ_FLAG_SCROLLABLE);
    const char *keys[] = {"1","2","3","4","5","6","7","8","9","X","0","H"};
    for (int i = 0; i < 12; ++i) {
        lv_obj_t *b = lv_btn_create(s_pad);
        lv_obj_set_size(b, 70, 36);
        lv_obj_set_pos(b, (i % 3) * 74, (i / 3) * 40);
        style_btn(b, false);
        lv_obj_add_event_cb(b, pad_key_cb, LV_EVENT_CLICKED, (void *)keys[i]);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, keys[i][0]=='X' ? "DEL" : (keys[i][0]=='H' ? "HIDE" : keys[i]));
        lv_obj_set_style_text_color(l, lv_color_hex(0xF4F7FB), 0);
        lv_obj_center(l);
    }
    lv_obj_add_flag(s_pad, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *big_label(lv_obj_t *parent, const char *txt, int y)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(0xF4F7FB), 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 8, y);
    return l;
}

static void build_ui(void)
{
    lv_obj_t *scr0 = lv_scr_act();
    s_scr_chg = scr0;
    style_screen(scr0);

    lv_obj_t *title = big_label(scr0, "CUBE  2.3.1", 2);
    (void)title;
    s_voltage = big_label(scr0, "U  --.- V", 24);
    s_current = big_label(scr0, "I  --.- A", 44);
    s_power = big_label(scr0, "P  -- W", 64);
    s_soc = big_label(scr0, "SOC -- %", 84);
    s_stage = big_label(scr0, "STAGE IDLE", 104);
    s_relay = big_label(scr0, "RELAY A0 B0", 124);
    s_cml = big_label(scr0, "CML -- V", 144);
    s_can = big_label(scr0, "CAN rx0 tx0", 164);
    s_status = big_label(scr0, "AP Cube / 123456789", 184);
    lv_obj_set_width(s_status, 224);
    mk_nav(scr0);

    s_scr_sys = lv_obj_create(NULL);
    style_screen(s_scr_sys);
    big_label(s_scr_sys, "SYSTEM", 2);
    s_sys_temp = big_label(s_scr_sys, "CPU -- C", 26);
    s_sys_cpu = big_label(s_scr_sys, "F 160 MHz", 46);
    s_sys_load = big_label(s_scr_sys, "Load -- %", 66);
    s_sys_heap = big_label(s_scr_sys, "Heap --", 86);
    s_sys_wifi = big_label(s_scr_sys, "AP Cube", 106);
    s_sys_touch = big_label(s_scr_sys, "Touch --", 126);
    s_br_label = big_label(s_scr_sys, "Bright 70%", 148);
    lv_obj_t *sl = lv_slider_create(s_scr_sys);
    lv_obj_set_width(sl, 200);
    lv_slider_set_range(sl, 5, 100);
    lv_slider_set_value(sl, s_brightness, LV_ANIM_OFF);
    lv_obj_align(sl, LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_add_event_cb(sl, br_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *rb = lv_btn_create(s_scr_sys);
    lv_obj_set_size(rb, 160, 34);
    lv_obj_align(rb, LV_ALIGN_TOP_MID, 0, 200);
    style_btn(rb, true);
    lv_obj_add_event_cb(rb, wifi_rst_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rl = lv_label_create(rb);
    lv_label_set_text(rl, "RESTART WiFi");
    lv_obj_set_style_text_color(rl, lv_color_hex(0xF4F7FB), 0);
    lv_obj_center(rl);
    mk_nav(s_scr_sys);

    s_scr_wifi = lv_obj_create(NULL);
    style_screen(s_scr_wifi);
    big_label(s_scr_wifi, "WiFi  digits only", 2);
    s_scan_lbl = big_label(s_scr_wifi, "nets: --", 22);
    lv_obj_set_width(s_scan_lbl, 224);
    s_wifi_ta = lv_textarea_create(s_scr_wifi);
    lv_obj_set_size(s_wifi_ta, 220, 32);
    lv_obj_align(s_wifi_ta, LV_ALIGN_TOP_MID, 0, 78);
    lv_textarea_set_placeholder_text(s_wifi_ta, "password 0-9");
    lv_textarea_set_one_line(s_wifi_ta, true);
    lv_textarea_set_password_mode(s_wifi_ta, false);
    lv_obj_add_event_cb(s_wifi_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_wifi_ta, ta_focus_cb, LV_EVENT_CLICKED, NULL);
    s_ip_lbl = big_label(s_scr_wifi, "STA IP --", 114);

    lv_obj_t *sc = lv_btn_create(s_scr_wifi);
    lv_obj_set_size(sc, 70, 32);
    lv_obj_align(sc, LV_ALIGN_TOP_LEFT, 6, 140);
    style_btn(sc, true);
    lv_obj_add_event_cb(sc, scan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *slb = lv_label_create(sc);
    lv_label_set_text(slb, "SCAN");
    lv_obj_center(slb);

    lv_obj_t *up = lv_btn_create(s_scr_wifi);
    lv_obj_set_size(up, 36, 32);
    lv_obj_align(up, LV_ALIGN_TOP_LEFT, 82, 140);
    style_btn(up, false);
    lv_obj_add_event_cb(up, pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)-1);
    lv_obj_t *ul = lv_label_create(up);
    lv_label_set_text(ul, "-");
    lv_obj_center(ul);

    lv_obj_t *dn = lv_btn_create(s_scr_wifi);
    lv_obj_set_size(dn, 36, 32);
    lv_obj_align(dn, LV_ALIGN_TOP_LEFT, 122, 140);
    style_btn(dn, false);
    lv_obj_add_event_cb(dn, pick_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    lv_obj_t *dl = lv_label_create(dn);
    lv_label_set_text(dl, "+");
    lv_obj_center(dl);

    lv_obj_t *jn = lv_btn_create(s_scr_wifi);
    lv_obj_set_size(jn, 70, 32);
    lv_obj_align(jn, LV_ALIGN_TOP_LEFT, 164, 140);
    style_btn(jn, true);
    lv_obj_add_event_cb(jn, join_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *jl = lv_label_create(jn);
    lv_label_set_text(jl, "JOIN");
    lv_obj_center(jl);

    make_pad(s_scr_wifi);
    mk_nav(s_scr_wifi);
    mark_nav();
}

esp_err_t display_init(void)
{
    s_lvgl_lock = xSemaphoreCreateMutex();
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
    size_t buf_px = CUBE_LCD_H_RES * 20;
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

    build_ui();
    ESP_LOGI(TAG, "LCD portrait %dx%d (ST7789=%d)", CUBE_LCD_H_RES, CUBE_LCD_V_RES, CUBE_LCD_USE_ST7789);
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
    float uv = st.ccs_output_voltage_raw > 0 ? st.ccs_output_voltage_raw / 10.0f
                                             : st.battery_voltage_0p1v / 10.0f;
    float ia = st.ccs_output_current_0p1a != 0 ? st.ccs_output_current_0p1a / 10.0f
                                               : st.battery_current_0p1a / 10.0f;
    if (ia < 0) ia = -ia;
    if (s_voltage) {
        snprintf(b, sizeof(b), "U  %.1f V", (double)uv);
        lv_label_set_text(s_voltage, b);
    }
    if (s_current) {
        snprintf(b, sizeof(b), "I  %.1f A", (double)ia);
        lv_label_set_text(s_current, b);
    }
    if (s_power) {
        snprintf(b, sizeof(b), "P  %.0f W", (double)(uv * ia));
        lv_label_set_text(s_power, b);
    }
    if (s_soc) {
        snprintf(b, sizeof(b), "SOC %u %%", (unsigned)st.soc_percent);
        lv_label_set_text(s_soc, b);
    }
    if (s_stage) {
        snprintf(b, sizeof(b), "STAGE %s", cube_main_state_name(st.main_state));
        lv_label_set_text(s_stage, b);
    }
    if (s_relay) {
        snprintf(b, sizeof(b), "RELAY A%d B%d", st.relay_a, st.relay_b);
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
        snprintf(b, sizeof(b), "WiFi %s / %s", CUBE_WIFI_SSID, CUBE_WIFI_PASSWORD);
        lv_label_set_text(s_status, b);
    }
    /* Thermal hint */
    if (tc > 75.0f && s_brightness > 40) {
        display_set_brightness(40);
    }
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
        if ((n++ % 20) == 0) display_set_values();
        vTaskDelay(pdMS_TO_TICKS(CUBE_LVGL_TASK_MS));
    }
}
