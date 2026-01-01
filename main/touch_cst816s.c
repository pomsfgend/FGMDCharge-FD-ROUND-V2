#include "touch_cst816s.h"
#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "config.h"
#include "app_state.h"

static const char *TAG = "TOUCH";
static bool s_ok;
static uint32_t s_last_probe_ms;

static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t len) {
    return i2c_master_write_read_device(CUBE_TOUCH_I2C_PORT, CUBE_TOUCH_ADDR, &reg, 1, buf, len, pdMS_TO_TICKS(50));
}

esp_err_t touch_init(void) {
#if !CUBE_TOUCH_ENABLED_DEFAULT
    s_ok = false;
    app_state_set_touch(false, false);
    ESP_LOGW(TAG, "Touch disabled by default until controller/rotation is confirmed");
    return ESP_OK;
#else
    i2c_config_t cfg = {0};
    cfg.mode = I2C_MODE_MASTER;
    cfg.sda_io_num = CUBE_TOUCH_SDA_GPIO;
    cfg.scl_io_num = CUBE_TOUCH_SCL_GPIO;
    cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    cfg.master.clk_speed = 100000;
    esp_err_t err = i2c_param_config(CUBE_TOUCH_I2C_PORT, &cfg);
    if (err != ESP_OK) return err;
    err = i2c_driver_install(CUBE_TOUCH_I2C_PORT, cfg.mode, 0, 0, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    uint8_t chip = 0;
    err = reg_read(0xA7, &chip, 1);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Touch controller ID=0x%02X at 0x%02X", (unsigned)chip, CUBE_TOUCH_ADDR);
        s_ok = true;
    } else {
        /* Some CST816S modules do not answer the ID register until the first
           touch event. Keep the input driver alive and retry later. */
        ESP_LOGW(TAG, "Initial touch probe unavailable: %s; lazy retry enabled", esp_err_to_name(err));
        s_ok = false;
    }
    s_last_probe_ms = (uint32_t)(esp_timer_get_time() / 1000ULL);
    app_state_set_touch(s_ok, false);
    return ESP_OK;
#endif
}

bool touch_is_available(void) { return s_ok; }

void touch_lvgl_read(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    static int16_t last_x = 0, last_y = 0;
    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
    if (!s_ok && (uint32_t)(now - s_last_probe_ms) >= 2000U) {
        uint8_t chip = 0;
        if (reg_read(0xA7, &chip, 1) == ESP_OK) {
            s_ok = true;
            ESP_LOGI(TAG, "Touch became available, ID=0x%02X", (unsigned)chip);
            app_state_set_touch(true, false);
        }
        s_last_probe_ms = now;
    }
    if (!s_ok) {
        data->state = LV_INDEV_STATE_REL;
        data->point.x = last_x;
        data->point.y = last_y;
        return;
    }
    uint8_t d[5] = {0};
    uint8_t reg = 0x02;
    esp_err_t err = i2c_master_write_read_device(CUBE_TOUCH_I2C_PORT, CUBE_TOUCH_ADDR, &reg, 1, d, sizeof(d), pdMS_TO_TICKS(30));
    if (err != ESP_OK) { s_ok = false; s_last_probe_ms = now; app_state_set_touch(false, false); data->state = LV_INDEV_STATE_REL; data->point.x = last_x; data->point.y = last_y; return; }
    bool pressed = (d[0] & 0x0F) != 0;
    int x = (((int)d[1] << 8) & 0x0F00) | d[2];
    int y = (((int)d[3] << 8) & 0x0F00) | d[4];
#if CUBE_TOUCH_SWAP_XY
    int tmp = x; x = y; y = tmp;
#endif
#if CUBE_TOUCH_INVERT_X
    x = CUBE_LCD_H_RES - 1 - x;
#endif
#if CUBE_TOUCH_INVERT_Y
    y = CUBE_LCD_V_RES - 1 - y;
#endif
    if (x < 0) {
        x = 0;
    }
    if (x >= CUBE_LCD_H_RES) {
        x = CUBE_LCD_H_RES - 1;
    }
    if (y < 0) {
        y = 0;
    }
    if (y >= CUBE_LCD_V_RES) {
        y = CUBE_LCD_V_RES - 1;
    }
    if (pressed) { last_x = (int16_t)x; last_y = (int16_t)y; }
    data->point.x = last_x;
    data->point.y = last_y;
    data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    app_state_set_touch(true, pressed);
}
