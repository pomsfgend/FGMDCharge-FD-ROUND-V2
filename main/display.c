#include "display.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "config.h"
#include "app_state.h"
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
    lv_obj_set_style_text_font(o, size >= 26 ? &lv_font_montserrat_28 : &lv_font_montserrat_14, 0);
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

static void ui_create(void) {
    /* Compact layout for 240x280 portrait (CST816 cube). */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x07111C), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = make_label(scr, "FGMDCharge", 14);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    /* Two metric cards: 112x64 with 8px margins fits 240 width cleanly. */
    (void)make_card(scr, 8, 24, 112, 64, "VOLTAGE", &s_voltage);
    (void)make_card(scr, 120, 24, 112, 64, "CURRENT", &s_current);

    s_power = make_label(scr, "POWER ---- W", 14);
    lv_obj_align(s_power, LV_ALIGN_TOP_LEFT, 10, 96);
    s_stage = make_label(scr, "STAGE --", 14);
    lv_obj_align(s_stage, LV_ALIGN_TOP_LEFT, 10, 118);
    s_request = make_label(scr, "REQ --.-V --.-A", 14);
    lv_obj_align(s_request, LV_ALIGN_TOP_LEFT, 10, 140);
    s_time = make_label(scr, "TIME ---- min", 14);
    lv_obj_align(s_time, LV_ALIGN_TOP_LEFT, 10, 162);
    s_status = make_label(scr, "STATUS --", 14);
    lv_obj_align(s_status, LV_ALIGN_TOP_LEFT, 10, 184);
    s_can = make_label(scr, "CAN 250k RX 0", 14);
    lv_obj_align(s_can, LV_ALIGN_TOP_LEFT, 10, 206);

    static char wifi_line[48];
    (void)snprintf(wifi_line, sizeof(wifi_line), "WiFi %s", CUBE_WIFI_SSID);
    lv_obj_t *hint = make_label(scr, wifi_line, 14);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_LEFT, 10, -28);

    lv_obj_t *hint2 = make_label(scr, CUBE_WIFI_PASSWORD[0] ? "WPA2 key set" : "OPEN AP 192.168.4.1", 14);
    lv_obj_align(hint2, LV_ALIGN_BOTTOM_LEFT, 10, -10);
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

    /* 240x280 panels on these cubes are almost always ST7789 (dump strings
       still said ILI9341). ST7789 needs Y gap + color invert. */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = CUBE_LCD_RST_GPIO,
        .bits_per_pixel = 16,
        /* BGR is the common byte order for ST7789/ILI modules with LVGL. */
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
    };
#if CUBE_LCD_USE_ST7789
    ESP_LOGW(TAG, "panel create ST7789 240x280...");
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
    /* ST7789 240x280 window is offset inside 240x320 GRAM. */
    (void)esp_lcd_panel_set_gap(s_panel, 0, 20);
    (void)esp_lcd_panel_invert_color(s_panel, true);
    if ((err = esp_lcd_panel_mirror(s_panel, false, false)) != ESP_OK) return err;
    if ((err = esp_lcd_panel_disp_on_off(s_panel, true)) != ESP_OK) return err;
    ESP_LOGW(TAG, "panel on (ST7789 gap_y=20 invert=1 BGR)");

    /* Smoke test: fill full screen so user sees life even if LVGL fonts fail. */
    {
        enum { LINE = 240 };
        uint16_t *line = heap_caps_malloc(LINE * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (line) {
            for (int i = 0; i < LINE; ++i) line[i] = 0xF800; /* red RGB565 */
            for (int y = 0; y < CUBE_LCD_V_RES; ++y) {
                (void)esp_lcd_panel_draw_bitmap(s_panel, 0, y, CUBE_LCD_H_RES, y + 1, line);
            }
            for (int i = 0; i < LINE; ++i) line[i] = 0x001F; /* blue */
            for (int y = 0; y < 40; ++y) {
                (void)esp_lcd_panel_draw_bitmap(s_panel, 0, y, CUBE_LCD_H_RES, y + 1, line);
            }
            heap_caps_free(line);
            ESP_LOGW(TAG, "panel smoke fill: red body + blue top bar");
            /* Let SPI DMA finish before LVGL registers the same callback path. */
            vTaskDelay(pdMS_TO_TICKS(50));
        }
    }

    /* Original FW: no backlight GPIO. GPIO2 is strapping — do NOT drive.
       Relays are GPIO6/GPIO7. Keep CUBE_LCD_BL_GPIO = -1.
       Use runtime int so the compiler never evaluates 1ULL << (-1). */
#if (CUBE_LCD_BL_GPIO >= 0) && (CUBE_LCD_BL_GPIO != 2)
    {
        const int bl_gpio = CUBE_LCD_BL_GPIO;
        gpio_config_t bl = {
            .pin_bit_mask = (1ULL << bl_gpio),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        if (gpio_config(&bl) == ESP_OK) {
            (void)gpio_set_level(bl_gpio, 1);
            ESP_LOGW(TAG, "backlight GPIO%d = ON (post-init)", bl_gpio);
        }
    }
#else
    ESP_LOGI(TAG, "backlight GPIO disabled (CUBE_LCD_BL_GPIO=%d)", CUBE_LCD_BL_GPIO);
#endif


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
    if (!lv_indev_drv_register(&s_touch_drv)) return ESP_FAIL;

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

    lv_label_set_text_fmt(s_stage, "STAGE: %s", cube_charge_stage_name(s.charger_stage));
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

    lv_timer_handler();
    display_unlock();
}

void display_task(void *arg) {
    (void)arg;
    for (;;) {
        display_set_values();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
