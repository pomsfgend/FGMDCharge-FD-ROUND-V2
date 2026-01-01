#pragma once

#include <stddef.h>
#include "esp_err.h"

#include "config.h"

esp_err_t settings_init(void);
esp_err_t settings_get_ble_name(char *out, size_t out_size);
esp_err_t settings_set_ble_name(const char *name);
