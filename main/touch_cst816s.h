#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "lvgl.h"

esp_err_t touch_init(void);
bool touch_is_available(void);
void touch_lvgl_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
