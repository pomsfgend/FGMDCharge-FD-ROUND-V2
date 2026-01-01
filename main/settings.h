#pragma once

#include <stddef.h>
#include "esp_err.h"

#define CUBE_NVS_NAMESPACE "storage"
#define CUBE_NVS_BLE_NAME "BLEname"
#define CUBE_NVS_ALLDATA "Alldata"

esp_err_t settings_init(void);
esp_err_t settings_get_ble_name(char *out, size_t out_size);
esp_err_t settings_set_ble_name(const char *name);
