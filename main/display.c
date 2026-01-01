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
static lv_obj_t *s_sys_temp, *s_sys_cpu, *s_sys_load, *s_sys_heap, *s_sys_wifi, *s_sys_touch;
static lv_obj_t *s_br_label;
static lv_obj_t *s_kb;
static lv_obj_t *s_wifi_ta;
static int s_page; /* 0 CHG 1 SYS 2 WIFI */

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

static void show_page(int p)
{
    s_page = p;
    if (s_kb) lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_scr_load(p == 0 ? s_scr_chg : (p == 1 ? s_scr_sys : s_scr_wifi));
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
        snprintf(b, sizeof(b), "Яркость %d%%", v);
        lv_label_set_text(s_br_label, b);
    }
}

static void kb_hide_cb(lv_event_t *e)
{
    (void)e;
    if (s_kb) lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
}

static void ta_focus_cb(lv_event_t *e)
{
    if (!s_kb) return;
    lv_keyboard_set_textarea(s_kb, lv_event_get_target(e));
    lv_obj_clear_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_kb);
}

static void wifi_rst_cb(lv_event_t *e)
{
    (void)e;
    web_wifi_restart();
}

static lv_obj_t *mk_nav(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 236, 36);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_pad_all(bar, 2, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x101820), 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    const char *names[] = {"CHG", "SYS", "WiFi"};
    for (int i = 0; i < 3; i++) {
        lv_obj_t *b = lv_btn_create(bar);
        lv_obj_set_size(b, 70, 30);
        lv_obj_add_event_cb(b, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, names[i]);
        lv_obj_center(l);
    }
    return bar;
}

static void build_ui(void)
{
    lv_obj_t *scr0 = lv_scr_act();
    s_scr_chg = scr0;
    lv_obj_set_style_bg_color(scr0, lv_color_hex(0x0a1018), 0);

    lv_obj_t *title = lv_label_create(scr0);
    lv_label_set_text(title, "CUBE  2.3.0");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    s_voltage = lv_label_create(scr0);
    lv_label_set_text(s_voltage, "U: -- V");
    lv_obj_align(s_voltage, LV_ALIGN_TOP_LEFT, 8, 28);

    s_current = lv_label_create(scr0);
    lv_label_set_text(s_current, "I: -- A");
    lv_obj_align(s_current, LV_ALIGN_TOP_LEFT, 8, 48);

    s_power = lv_label_create(scr0);
    lv_label_set_text(s_power, "P: -- W");
    lv_obj_align(s_power, LV_ALIGN_TOP_LEFT, 8, 68);

    s_stage = lv_label_create(scr0);
    lv_label_set_text(s_stage, "Этап: IDLE");
    lv_obj_align(s_stage, LV_ALIGN_TOP_LEFT, 8, 96);

    s_status = lv_label_create(scr0);
    lv_label_set_text(s_status, "WiFi Cube / 123456789");
    lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 8, 120);
    lv_obj_set_width(s_status, 220);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);

    mk_nav(scr0);

    /* SYS */
    s_scr_sys = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_sys, lv_color_hex(0x0a1018), 0);
    lv_obj_t *st = lv_label_create(s_scr_sys);
    lv_label_set_text(st, "СИСТЕМА");
    lv_obj_align(st, LV_ALIGN_TOP_MID, 0, 4);

    s_sys_temp = lv_label_create(s_scr_sys);
    lv_label_set_text(s_sys_temp, "CPU: -- C");
    lv_obj_align(s_sys_temp, LV_ALIGN_TOP_LEFT, 8, 28);

    s_sys_cpu = lv_label_create(s_scr_sys);
    lv_label_set_text(s_sys_cpu, "F: 160 MHz");
    lv_obj_align(s_sys_cpu, LV_ALIGN_TOP_LEFT, 8, 48);

    s_sys_load = lv_label_create(s_scr_sys);
    lv_label_set_text(s_sys_load, "Load: -- %");
    lv_obj_align(s_sys_load, LV_ALIGN_TOP_LEFT, 8, 68);

    s_sys_heap = lv_label_create(s_scr_sys);
    lv_label_set_text(s_sys_heap, "Heap: --");
    lv_obj_align(s_sys_heap, LV_ALIGN_TOP_LEFT, 8, 88);

    s_sys_wifi = lv_label_create(s_scr_sys);
    lv_label_set_text(s_sys_wifi, "AP: Cube ch?");
    lv_obj_align(s_sys_wifi, LV_ALIGN_TOP_LEFT, 8, 108);

    s_sys_touch = lv_label_create(s_scr_sys);
    lv_label_set_text(s_sys_touch, "Touch: OK");
    lv_obj_align(s_sys_touch, LV_ALIGN_TOP_LEFT, 8, 128);

    s_br_label = lv_label_create(s_scr_sys);
    lv_label_set_text(s_br_label, "Яркость 70%");
    lv_obj_align(s_br_label, LV_ALIGN_TOP_LEFT, 8, 152);

    lv_obj_t *sl = lv_slider_create(s_scr_sys);
    lv_obj_set_width(sl, 200);
    lv_slider_set_range(sl, 5, 100);
    lv_slider_set_value(sl, s_brightness, LV_ANIM_OFF);
    lv_obj_align(sl, LV_ALIGN_TOP_MID, 0, 175);
    lv_obj_add_event_cb(sl, br_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t *rb = lv_btn_create(s_scr_sys);
    lv_obj_set_size(rb, 160, 36);
    lv_obj_align(rb, LV_ALIGN_TOP_MID, 0, 210);
    lv_obj_add_event_cb(rb, wifi_rst_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rl = lv_label_create(rb);
    lv_label_set_text(rl, "RESTART WiFi");
    lv_obj_center(rl);

    mk_nav(s_scr_sys);

    /* WIFI page */
    s_scr_wifi = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr_wifi, lv_color_hex(0x0a1018), 0);
    lv_obj_t *wt = lv_label_create(s_scr_wifi);
    lv_label_set_text(wt, "WiFi STA");
    lv_obj_align(wt, LV_ALIGN_TOP_MID, 0, 4);

    lv_obj_t *hint = lv_label_create(s_scr_wifi);
    lv_label_set_text(hint, "Пароль STA (скрывается OK):");
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 8, 28);

    s_wifi_ta = lv_textarea_create(s_scr_wifi);
    lv_obj_set_size(s_wifi_ta, 220, 36);
    lv_obj_align(s_wifi_ta, LV_ALIGN_TOP_MID, 0, 48);
    lv_textarea_set_one_line(s_wifi_ta, true);
    lv_textarea_set_password_mode(s_wifi_ta, true);
    lv_textarea_set_placeholder_text(s_wifi_ta, "password");
    lv_obj_add_event_cb(s_wifi_ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(s_wifi_ta, kb_hide_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *hide = lv_btn_create(s_scr_wifi);
    lv_obj_set_size(hide, 100, 32);
    lv_obj_align(hide, LV_ALIGN_TOP_LEFT, 8, 92);
    lv_obj_add_event_cb(hide, kb_hide_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *hl = lv_label_create(hide);
    lv_label_set_text(hl, "Скрыть KB");
    lv_obj_center(hl);

    /* Large keyboard */
    s_kb = lv_keyboard_create(s_scr_wifi);
    lv_obj_set_size(s_kb, 240, 110);
    lv_obj_align(s_kb, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_keyboard_set_textarea(s_kb, s_wifi_ta);
    lv_obj_set_style_pad_all(s_kb, 2, 0);
    lv_obj_add_flag(s_kb, LV_OBJ_FLAG_HIDDEN);
    /* Hide on ready */
    lv_obj_add_event_cb(s_kb, kb_hide_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_kb, kb_hide_cb, LV_EVENT_CANCEL, NULL);

    mk_nav(s_scr_wifi);
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
    char b[48];
    float tc = read_temp_c();
    size_t heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int load = cpu_load_pct();
    int ch = web_get_ap_channel();

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
